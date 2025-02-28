/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2015-2016 Symless Ltd.
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

#include "server/ClientProxy1_5.h"

namespace inputleap {

class Server;
class IEventQueue;

//! Proxy for client implementing protocol version 1.6
class ClientProxy1_6 : public ClientProxy1_5 {
public:
    ClientProxy1_6(const std::string& name, inputleap::IStream* adoptedStream, Server* server,
                   IEventQueue* events);
    ~ClientProxy1_6() override;

    void setClipboard(ClipboardID id, const IClipboard* clipboard) override;
    bool recvClipboard() override;

private:
    void handle_clipboard_sending_event(const Event& event);

private:
    IEventQueue* m_events;
};

} // namespace inputleap
