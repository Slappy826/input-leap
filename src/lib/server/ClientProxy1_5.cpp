/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2013-2016 Symless Ltd.
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

#include "server/ClientProxy1_5.h"

#include "server/Server.h"
#include "inputleap/FileChunk.h"
#include "inputleap/StreamChunker.h"
#include "inputleap/ProtocolUtil.h"
#include "io/IStream.h"
#include "base/Log.h"

#include <sstream>

namespace inputleap {

ClientProxy1_5::ClientProxy1_5(const std::string& name, inputleap::IStream* stream, Server* server,
                               IEventQueue* events) :
    ClientProxy1_4(name, stream, server, events),
    m_events(events)
{
    m_events->add_handler(EventType::FILE_KEEPALIVE, this,
                          [this](const auto& e){ handle_keep_alive(); });
}

ClientProxy1_5::~ClientProxy1_5()
{
    m_events->removeHandler(EventType::FILE_KEEPALIVE, this);
}

void ClientProxy1_5::sendDragInfo(std::uint32_t fileCount, const char* info, size_t size)
{
    std::string data(info, size);

    ProtocolUtil::writef(getStream(), kMsgDDragInfo, fileCount, &data);
}

void ClientProxy1_5::fileChunkSending(std::uint8_t mark, const char* data, size_t dataSize)
{
    FileChunk::send(getStream(), mark, data, dataSize);
}

bool ClientProxy1_5::parseMessage(const std::uint8_t* code)
{
    if (memcmp(code, kMsgDFileTransfer, 4) == 0) {
        fileChunkReceived();
    }
    else if (memcmp(code, kMsgDDragInfo, 4) == 0) {
        dragInfoReceived();
    }
    else {
        return ClientProxy1_4::parseMessage(code);
    }

    return true;
}

void
ClientProxy1_5::fileChunkReceived()
{
    Server* server = getServer();
    int result = FileChunk::assemble(
                    getStream(),
                    server->getReceivedFileData(),
                    server->getExpectedFileSize());


    if (result == kFinish) {
        m_events->add_event(EventType::FILE_RECEIVE_COMPLETED, server);
    }
    else if (result == kStart) {
        if (server->getFakeDragFileList().size() > 0) {
            std::string filename = server->getFakeDragFileList().at(0).getFilename();
            LOG((CLOG_DEBUG "start receiving %s", filename.c_str()));
        }
    }
}

void
ClientProxy1_5::dragInfoReceived()
{
    // parse
    std::uint32_t fileNum = 0;
    std::string content;
    ProtocolUtil::readf(getStream(), kMsgDDragInfo + 4, &fileNum, &content);

    m_server->dragInfoReceived(fileNum, content);
}

} // namespace inputleap
