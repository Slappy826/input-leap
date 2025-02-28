/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
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

#pragma once

#include "net/IListenSocket.h"
#include "net/ISocketMultiplexerJob.h"
#include "arch/IArchNetwork.h"

#include <mutex>

namespace inputleap {

class IEventQueue;
class SocketMultiplexer;

//! TCP listen socket
/*!
A listen socket using TCP.
*/
class TCPListenSocket : public IListenSocket {
public:
    TCPListenSocket(IEventQueue* events, SocketMultiplexer* socketMultiplexer, IArchNetwork::EAddressFamily family);
    virtual ~TCPListenSocket();

    // ISocket overrides
    void bind(const NetworkAddress&) override;
    void close() override;
    void* getEventTarget() const override;

    // IListenSocket overrides
    std::unique_ptr<IDataSocket> accept() override;

protected:
    void setListeningJob();

public:
    MultiplexerJobStatus serviceListening(ISocketMultiplexerJob*, bool, bool, bool);

protected:
    ArchSocket m_socket;
    std::mutex mutex_;
    IEventQueue* m_events;
    SocketMultiplexer* m_socketMultiplexer;
};

} // namespace inputleap
