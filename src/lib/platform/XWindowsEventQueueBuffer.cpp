/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2004 Chris Schoeneman
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <cassert>
#include "platform/XWindowsEventQueueBuffer.h"

#include "mt/Thread.h"
#include "base/Event.h"
#include "base/IEventQueue.h"

#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

namespace inputleap {

class EventQueueTimer { };


//
// XWindowsEventQueueBuffer
//

XWindowsEventQueueBuffer::XWindowsEventQueueBuffer(IXWindowsImpl* impl,
        Display* display, Window window, IEventQueue* events) :
    m_display(display),
    m_window(window),
    m_waiting(false),
    m_events(events)
{
    m_impl = impl;
    assert(m_display != nullptr);
    assert(m_window  != None);

    m_userEvent = m_impl->XInternAtom(m_display, "INPUTLEAP_USER_EVENT", False);
    // set up for pipe hack
    int result = pipe(m_pipefd);
    assert(result == 0);

    int pipeflags;
    pipeflags = fcntl(m_pipefd[0], F_GETFL);
    fcntl(m_pipefd[0], F_SETFL, pipeflags | O_NONBLOCK);
    pipeflags = fcntl(m_pipefd[1], F_GETFL);
    fcntl(m_pipefd[1], F_SETFL, pipeflags | O_NONBLOCK);
}

XWindowsEventQueueBuffer::~XWindowsEventQueueBuffer()
{
    // release pipe hack resources
    close(m_pipefd[0]);
    close(m_pipefd[1]);
}

int XWindowsEventQueueBuffer::getPendingCountLocked()
{
    std::lock_guard<std::mutex> lock(mutex_);
    // work around a bug in old libx11 which causes the first XPending not to read events under
    // certain conditions. The issue happens when libx11 has not yet received replies for all
    // flushed events. In that case, internally XPending will not try to process received events
    // as the reply for the last event was not found. As a result, XPending will return the number
    // of pending events without regard to the events it has just read.
    // https://gitlab.freedesktop.org/xorg/lib/libx11/-/merge_requests/1 fixes this on libx11 side.
    m_impl->XPending(m_display);
    return m_impl->XPending(m_display);
}

void
XWindowsEventQueueBuffer::waitForEvent(double dtimeout)
{
    Thread::testCancel();

    // clear out the pipe in preparation for waiting.

    char buf[16];
    ssize_t read_response = read(m_pipefd[0], buf, 15);
    if (read_response < 0)
    {
        // todo: handle read response
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // we're now waiting for events
        m_waiting = true;

        // push out pending events
        flush();
    }
    // calling flush may have queued up a new event.
    if (!XWindowsEventQueueBuffer::isEmpty()) {
        Thread::testCancel();
        return;
    }

    // use poll() to wait for a message from the X server or for timeout.
    // this is a good deal more efficient than polling and sleeping.
    struct pollfd pfds[2];
    pfds[0].fd     = ConnectionNumber(m_display);
    pfds[0].events = POLLIN;
    pfds[1].fd     = m_pipefd[0];
    pfds[1].events = POLLIN;
    int timeout    = (dtimeout < 0.0) ? -1 :
                        static_cast<int>(1000.0 * dtimeout);
    int remaining  =  timeout;
    int retval     =  0;
    // It's possible that the X server has queued events locally
    // in xlib's event buffer and not pushed on to the fd. Hence we
    // can't simply monitor the fd as we may never be woken up.
    // ie addEvent calls flush, XFlush may not send via the fd hence
    // there is an event waiting to be sent but we must exit the poll
    // before it can.
    // Instead we poll for a brief period of time (so if events
    // queued locally in the xlib buffer can be processed)
    // and continue doing this until timeout is reached.
    // The human eye can notice 60hz (ansi) which is 16ms, however
    // we want to give the cpu a chance s owe up this to 25ms
#define TIMEOUT_DELAY 25

    while (((dtimeout < 0.0) || (remaining > 0)) && getPendingCountLocked() == 0 && retval == 0) {
        retval = poll(pfds, 2, TIMEOUT_DELAY); //16ms = 60hz, but we make it > to play nicely with the cpu
        if (pfds[1].revents & POLLIN) {
            read_response = read(m_pipefd[0], buf, 15);
            if (read_response < 0)
            {
                // todo: handle read response
            }
        }
        remaining-=TIMEOUT_DELAY;
    }

    {
        // we're no longer waiting for events
        std::lock_guard<std::mutex> lock(mutex_);
        m_waiting = false;
    }

    Thread::testCancel();
}

IEventQueueBuffer::Type XWindowsEventQueueBuffer::getEvent(Event& event, std::uint32_t& dataID)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // push out pending events
    flush();

    // get next event
    m_impl->XNextEvent(m_display, &m_event);

    // process event
    if (m_event.xany.type == ClientMessage &&
        m_event.xclient.message_type == m_userEvent) {
        dataID = static_cast<std::uint32_t>(m_event.xclient.data.l[0]);
        return kUser;
    }
    else {
        event = Event(EventType::SYSTEM, m_events->getSystemTarget(),
                      create_event_data<XEvent*>(&m_event));
        return kSystem;
    }
}

bool XWindowsEventQueueBuffer::addEvent(std::uint32_t dataID)
{
    // prepare a message
    XEvent xevent;
    xevent.xclient.type         = ClientMessage;
    xevent.xclient.window       = m_window;
    xevent.xclient.message_type = m_userEvent;
    xevent.xclient.format       = 32;
    xevent.xclient.data.l[0]    = static_cast<long>(dataID);

    // save the message
    std::lock_guard<std::mutex> lock(mutex_);
    m_postedEvents.push_back(xevent);

    // if we're currently waiting for an event then send saved events to
    // the X server now.  if we're not waiting then some other thread
    // might be using the display connection so we can't safely use it
    // too.
    if (m_waiting) {
        flush();
        // Send a character through the round-trip pipe to wake a thread
        // that is waiting for a ConnectionNumber() socket to be readable.
        // The flush call can read incoming data from the socket and put
        // it in Xlib's input buffer.  That sneaks it past the other thread.
        ssize_t write_response = write(m_pipefd[1], "!", 1);
        if (write_response < 0)
        {
            // todo: handle write response
        }
    }

    return true;
}

bool
XWindowsEventQueueBuffer::isEmpty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return (m_impl->XPending(m_display) == 0 );
}

EventQueueTimer*
XWindowsEventQueueBuffer::newTimer(double, bool) const
{
    return new EventQueueTimer;
}

void
XWindowsEventQueueBuffer::deleteTimer(EventQueueTimer* timer) const
{
    delete timer;
}

void
XWindowsEventQueueBuffer::flush()
{
    // note -- mutex_ must be locked on entry

    // flush the posted event list to the X server
    for (size_t i = 0; i < m_postedEvents.size(); ++i) {
        m_impl->XSendEvent(m_display, m_window, False, 0, &m_postedEvents[i]);
    }
    m_impl->XFlush(m_display);
    m_postedEvents.clear();
}

} // namespace inputleap
