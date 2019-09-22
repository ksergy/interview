#include "receiver.h"

#include <cstring>
#include <cstdlib>

const Receiver::ChunkProcessor Receiver::CHUNK_PROCESSOR[_PT_SIZE] = {
    &Receiver::packetStarted,
    &Receiver::packetContinueBinary,
    &Receiver::packetContinueText
};

const Receiver::ReceivedPktCallback Receiver::RECEIVED_PKT_CALLBACK[_PT_SIZE] = {
    nullptr,
    &ICallback::BinaryPacket,
    &ICallback::TextPacket
};

const Receiver::BinaryPacketProcessor Receiver::BINARY_PACKET_PROCESSOR[_BPS_COUNT] = {
    nullptr,
    &Receiver::binaryPacketHeader,
    &Receiver::binaryPacketData
};

#define _BINARY_PKT_START "\x24"
#define _TEXT_PKT_FINISH "\r\n\r\n"

const char* Receiver::BINARY_PKT_START = _BINARY_PKT_START;
size_t Receiver::BINARY_PKT_START_LEN = strlen(_BINARY_PKT_START);
const char* Receiver::TEXT_PKT_FINISH = _TEXT_PKT_FINISH;
size_t Receiver::TEXT_PKT_FINISH_LEN = strlen(_TEXT_PKT_FINISH);

#undef _BINARY_PKT_START
#undef _TEXT_PKT_FINISH


Receiver::Receiver(ICallback* callback)
: _callback(callback),
  _currentPacket{PT_NONE, 0, nullptr, 0, 0, 0, BPS_NONE}
{
}

Receiver::~Receiver() {
    if (_currentPacket.data) {
        free(_currentPacket.data);
    }
}

void Receiver::Receive(const char* data, unsigned int size)
{
    anotherChunkReceived(Chunk{data, size});
}

void Receiver::anotherChunkReceived(const Receiver::Chunk& chunk)
{
    if (chunk.size == 0) {
        return;
    }

    append(chunk);

    anotherChunkReceivedImpl();
}

void Receiver::anotherChunkReceivedImpl()
{
    auto processor = CHUNK_PROCESSOR[_currentPacket.type];

    (this->*processor)();
}

size_t Receiver::dataAvailable() const
{
    return _currentPacket.actualDataSize - _currentPacket.offset;
}

void Receiver::shift()
{
    memmove(_currentPacket.data, getData(), dataAvailable());

    _currentPacket.actualDataSize -= _currentPacket.offset;
    _currentPacket.offset = 0;
}

size_t Receiver::dataRead() const
{
    return _currentPacket.offset;
}

size_t Receiver::dataRead(size_t amount)
{
    _currentPacket.offset += amount;
    return _currentPacket.offset;
}

const char * Receiver::getData() const
{
    return _currentPacket.data + _currentPacket.offset;
}

bool Receiver::ensureDataAvailable(size_t required)
{
    return dataAvailable() >= required;
}

void Receiver::append(const Receiver::Chunk& chunk)
{
    if (_currentPacket.reservedDataSize < _currentPacket.actualDataSize + chunk.size) {
        _currentPacket.data = reinterpret_cast<char *>(realloc(_currentPacket.data, _currentPacket.actualDataSize + chunk.size));
        _currentPacket.reservedDataSize = _currentPacket.actualDataSize + chunk.size;
    }

    memcpy(_currentPacket.data + _currentPacket.actualDataSize, chunk.data, chunk.size);
    _currentPacket.actualDataSize += chunk.size;
}

int32_t Receiver::lsbToHost(int32_t v)
{
    return v;
}

const char * Receiver::memmem(const char* haystack, size_t haystackLen,
                              const char* needle, size_t needleLen) const
{
    const char * const haystackLimit = haystack + haystackLen;
    const char * const needleLimit = needle + needleLen;

    for (const char *ptr = haystack; ptr < haystackLimit; ++ptr) {
        const char *needlePtr = needle;

        for (const char *ptr2 = ptr; ptr2 < haystackLimit && needlePtr < needleLimit; ++ptr2, ++needlePtr) {
            if (*needlePtr != *ptr2) {
                break;
            }
        }

        if (needlePtr == needleLimit) {
            return ptr;
        }
    }

    return nullptr;
}


void Receiver::packetStarted()
{
    /* parse packet type */

    if (!ensureDataAvailable(BINARY_PKT_START_LEN)) {
        // just wait a bit
        return;
    }

    if (0 == memcmp(BINARY_PKT_START, _currentPacket.data, BINARY_PKT_START_LEN)) {
        _currentPacket.type = PT_BINARY;
        _currentPacket.binaryPacketState = BPS_HEADER;

        dataRead(BINARY_PKT_START_LEN);
    } else {
        _currentPacket.type = PT_TEXT;
    }

    anotherChunkReceivedImpl();
}

void Receiver::packetContinueBinary()
{
    auto processor = BINARY_PACKET_PROCESSOR[_currentPacket.binaryPacketState];

    (this->*processor)();
}

void Receiver::binaryPacketHeader()
{
    /* parse packet size */
    if (!ensureDataAvailable(sizeof(_currentPacket.packetSize))) {
        // just wait a bit
        return;
    }

    memcpy(&_currentPacket.packetSize, getData(), sizeof(_currentPacket.packetSize));
    dataRead(sizeof(_currentPacket.packetSize));

    _currentPacket.packetSize = lsbToHost(_currentPacket.packetSize);

    _currentPacket.binaryPacketState = BPS_DATA;

    packetContinueBinary();
}

void Receiver::binaryPacketData()
{
    if (!ensureDataAvailable(_currentPacket.packetSize)) {
        // just wait a bit
        return;
    }

    packetFinished(_currentPacket.packetSize, _currentPacket.packetSize);
}


void Receiver::packetContinueText()
{
    const char *found = memmem(getData(), dataAvailable(), TEXT_PKT_FINISH, TEXT_PKT_FINISH_LEN);

    if (found) {
        packetFinished(found - getData(), found - getData() + TEXT_PKT_FINISH_LEN);
    }
}

void Receiver::packetFinished(size_t packetSize, size_t sizeToRead)
{
    auto callback = RECEIVED_PKT_CALLBACK[_currentPacket.type];
    (_callback->*callback)(getData(), packetSize);

    dataRead(sizeToRead);
    shift();
    _currentPacket.type = PT_NONE;
}
