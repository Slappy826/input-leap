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

#include "server/ClientListener.h"

#include "server/ClientProxy.h"
#include "server/ClientProxyUnknown.h"
#include "inputleap/PacketStreamFilter.h"
#include "net/IDataSocket.h"
#include "net/IListenSocket.h"
#include "net/ISocketFactory.h"
#include "net/XSocket.h"
#include "base/Log.h"
#include "base/IEventQueue.h"

namespace inputleap {

ClientListener::ClientListener(const NetworkAddress& address,
                ISocketFactory* socketFactory,
                IEventQueue* events,
                               ConnectionSecurityLevel security_level) :
    m_socketFactory(socketFactory),
    m_server(nullptr),
    m_events(events),
    security_level_{security_level}
{
    assert(m_socketFactory != nullptr);

    try {
        m_listen = m_socketFactory->createListen(ARCH->getAddrFamily(address.getAddress()),
                                                 security_level);

        // setup event handler
        m_events->add_handler(EventType::LISTEN_SOCKET_CONNECTING, m_listen,
                              [this](const auto& e){ handle_client_connecting(); });

        // bind listen address
        LOG((CLOG_DEBUG1 "binding listen socket"));
        m_listen->bind(address);
    }
    catch (XSocketAddressInUse&) {
        cleanupListenSocket();
        delete m_socketFactory;
        throw;
    }
    catch (XBase&) {
        cleanupListenSocket();
        delete m_socketFactory;
        throw;
    }
    LOG((CLOG_DEBUG1 "listening for clients"));
}

ClientListener::~ClientListener()
{
    LOG((CLOG_DEBUG1 "stop listening for clients"));

    // discard already connected clients
    for (NewClients::iterator index = m_newClients.begin();
                                index != m_newClients.end(); ++index) {
        ClientProxyUnknown* client = *index;
        m_events->removeHandler(EventType::CLIENT_PROXY_UNKNOWN_SUCCESS, client);
        m_events->removeHandler(EventType::CLIENT_PROXY_UNKNOWN_FAILURE, client);
        m_events->removeHandler(EventType::CLIENT_PROXY_DISCONNECTED, client);
        delete client;
    }

    // discard waiting clients
    ClientProxy* client = getNextClient();
    while (client != nullptr) {
        delete client;
        client = getNextClient();
    }

    m_events->removeHandler(EventType::LISTEN_SOCKET_CONNECTING, m_listen);
    cleanupListenSocket();
    cleanupClientSockets();
    delete m_socketFactory;
}

void
ClientListener::setServer(Server* server)
{
    assert(server != nullptr);
    m_server = server;
}

ClientProxy*
ClientListener::getNextClient()
{
    ClientProxy* client = nullptr;
    if (!m_waitingClients.empty()) {
        client = m_waitingClients.front();
        m_waitingClients.pop_front();
        m_events->removeHandler(EventType::CLIENT_PROXY_DISCONNECTED, client);
    }
    return client;
}

void ClientListener::handle_client_connecting()
{
    // accept client connection
    auto socket = m_listen->accept();

    if (!socket) {
        return;
    }

    auto socket_ptr = socket.get();
    client_sockets_.insert(std::move(socket));

    m_events->add_handler(EventType::CLIENT_LISTENER_ACCEPTED, socket_ptr->getEventTarget(),
                          [this, socket_ptr](const auto& e)
    {
        handle_client_accepted(socket_ptr);
    });

    // When using non SSL, server accepts clients immediately, while SSL
    // has to call secure accept which may require retry
    if (security_level_ == ConnectionSecurityLevel::PLAINTEXT) {
        m_events->add_event(EventType::CLIENT_LISTENER_ACCEPTED, socket_ptr->getEventTarget());
    }
}

void
ClientListener::handle_client_accepted(IDataSocket* socket)
{
    LOG((CLOG_NOTE "accepted client connection"));

    // filter socket messages, including a packetizing filter
    inputleap::IStream* stream = new PacketStreamFilter(m_events, socket, false);
    assert(m_server != nullptr);

    // create proxy for unknown client
    ClientProxyUnknown* client = new ClientProxyUnknown(stream, 30.0, m_server, m_events);

    m_newClients.insert(client);

    // watch for events from unknown client
    m_events->add_handler(EventType::CLIENT_PROXY_UNKNOWN_SUCCESS, client,
                          [this, client](const auto& e){ handle_unknown_client(client); });
    m_events->add_handler(EventType::CLIENT_PROXY_UNKNOWN_FAILURE, client,
                          [this, client](const auto& e){ handle_unknown_client(client); });
}

void ClientListener::handle_unknown_client(ClientProxyUnknown* unknownClient)
{
    // we should have the client in our new client list
    assert(m_newClients.count(unknownClient) == 1);

    // get the real client proxy and install it
    ClientProxy* client = unknownClient->orphanClientProxy();
    if (client != nullptr) {
        // handshake was successful
        m_waitingClients.push_back(client);
        m_events->add_event(EventType::CLIENT_LISTENER_CONNECTED, this);

        // watch for client to disconnect while it's in our queue
        m_events->add_handler(EventType::CLIENT_PROXY_DISCONNECTED, client,
                              [this, client](const auto& e) { handle_client_disconnected(client); });
    } else {
        auto* stream = unknownClient->getStream();
        if (stream) {
            stream->close();
        }
    }

    // now finished with unknown client
    m_events->removeHandler(EventType::CLIENT_PROXY_UNKNOWN_SUCCESS, client);
    m_events->removeHandler(EventType::CLIENT_PROXY_UNKNOWN_FAILURE, client);
    m_newClients.erase(unknownClient);

    delete unknownClient;
}

void ClientListener::handle_client_disconnected(ClientProxy* client)
{
    // find client in waiting clients queue
    for (WaitingClients::iterator i = m_waitingClients.begin(),
                            n = m_waitingClients.end(); i != n; ++i) {
        if (*i == client) {
            m_waitingClients.erase(i);
            m_events->removeHandler(EventType::CLIENT_PROXY_DISCONNECTED, client);

            // pull out the socket before deleting the client so
            // we know which socket we no longer need
            IDataSocket* socket = static_cast<IDataSocket*>(client->getStream());
            delete client;
            client_sockets_.erase(socket);

            break;
        }
    }
}

void
ClientListener::cleanupListenSocket()
{
    delete m_listen;
}

void
ClientListener::cleanupClientSockets()
{
    client_sockets_.clear();
}

} // namespace inputleap
