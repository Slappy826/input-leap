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

#include "inputleap/StreamChunker.h"

#include "inputleap/FileChunk.h"
#include "inputleap/ClipboardChunk.h"
#include "inputleap/protocol_types.h"
#include "base/EventTypes.h"
#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/EventTypes.h"
#include "base/Log.h"
#include "base/String.h"

#include <fstream>
#include <stdexcept>

namespace inputleap {

static const size_t g_chunkSize = 32 * 1024; //32kb

bool StreamChunker::s_isChunkingFile = false;
bool StreamChunker::s_interruptFile = false;

void
StreamChunker::sendFile(const char* filename,
                IEventQueue* events,
                void* eventTarget)
{
    s_isChunkingFile = true;

    std::fstream file(filename, std::ios::in | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file");
    }

    // check file size
    file.seekg (0, std::ios::end);
    size_t size = static_cast<size_t>(file.tellg());

    // send first message (file size)
    std::string fileSize = inputleap::string::sizeTypeToString(size);
    FileChunk* sizeMessage = FileChunk::start(fileSize);

    events->add_event(EventType::FILE_CHUNK_SENDING, eventTarget,
                      create_event_data<FileChunk>(*sizeMessage));
    delete sizeMessage;

    // send chunk messages with a fixed chunk size
    size_t sentLength = 0;
    size_t chunkSize = g_chunkSize;
    file.seekg (0, std::ios::beg);

    while (true) {
        if (s_interruptFile) {
            s_interruptFile = false;
            LOG((CLOG_DEBUG "file transmission interrupted"));
            break;
        }

        events->add_event(EventType::FILE_KEEPALIVE, eventTarget);

        // make sure we don't read too much from the mock data.
        if (sentLength + chunkSize > size) {
            chunkSize = size - sentLength;
        }

        char* chunkData = new char[chunkSize];
        file.read(chunkData, chunkSize);
        std::uint8_t* data = reinterpret_cast<std::uint8_t*>(chunkData);
        FileChunk* fileChunk = FileChunk::data(data, chunkSize);
        delete[] chunkData;

        events->add_event(EventType::FILE_CHUNK_SENDING, eventTarget,
                          create_event_data<FileChunk>(*fileChunk));
        delete fileChunk;

        sentLength += chunkSize;
        file.seekg (sentLength, std::ios::beg);

        if (sentLength == size) {
            break;
        }
    }

    // send last message
    FileChunk* end = FileChunk::end();

    events->add_event(EventType::FILE_CHUNK_SENDING, eventTarget,
                      create_event_data<FileChunk>(*end));
    delete end;

    file.close();

    s_isChunkingFile = false;
}

void
StreamChunker::sendClipboard(
                std::string& data,
                size_t size,
                ClipboardID id,
                std::uint32_t sequence,
                IEventQueue* events,
                void* eventTarget)
{
    // send first message (data size)
    std::string dataSize = inputleap::string::sizeTypeToString(size);
    ClipboardChunk* sizeMessage = ClipboardChunk::start(id, sequence, dataSize);

    events->add_event(EventType::CLIPBOARD_SENDING, eventTarget,
                      create_event_data<ClipboardChunk>(*sizeMessage));
    delete sizeMessage;

    // send clipboard chunk with a fixed size
    size_t sentLength = 0;
    size_t chunkSize = g_chunkSize;

    while (true) {
        events->add_event(EventType::FILE_KEEPALIVE, eventTarget);

        // make sure we don't read too much from the mock data.
        if (sentLength + chunkSize > size) {
            chunkSize = size - sentLength;
        }

        std::string chunk(data.substr(sentLength, chunkSize).c_str(), chunkSize);
        ClipboardChunk* dataChunk = ClipboardChunk::data(id, sequence, chunk);

        events->add_event(EventType::CLIPBOARD_SENDING, eventTarget,
                          create_event_data<ClipboardChunk>(*dataChunk));
        delete dataChunk;

        sentLength += chunkSize;
        if (sentLength == size) {
            break;
        }
    }

    // send last message
    ClipboardChunk* end = ClipboardChunk::end(id, sequence);

    events->add_event(EventType::CLIPBOARD_SENDING, eventTarget,
                      create_event_data<ClipboardChunk>(*end));
    delete end;

    LOG((CLOG_DEBUG "sent clipboard size=%d", sentLength));
}

void
StreamChunker::interruptFile()
{
    if (s_isChunkingFile) {
        s_interruptFile = true;
        LOG((CLOG_INFO "previous dragged file has become invalid"));
    }
}

} // namespace inputleap
