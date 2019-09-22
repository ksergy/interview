#ifndef ISS_RECEIVER_H
#define ISS_RECEIVER_H 1

#include "iface.h"

#include <cstddef>
#include <cstdint>

struct Receiver : public IReceiver {
private:
    /************************ types ************************/
    enum PacketType {
        PT_NONE     = 0,
        PT_BINARY,
        PT_TEXT,

        _PT_SIZE
    };

    enum BinaryPacketState {
        BPS_NONE    = 0,
        BPS_HEADER,
        BPS_DATA,

        _BPS_COUNT
    };

    struct PacketDescr {
        /* data from header */
        PacketType type;
        uint32_t packetSize;        // only used for binary pkt

        char *data;                 // actual data

        // offset < actualDataSize < reservedDataSize
        size_t actualDataSize;      // mem used
        size_t reservedDataSize;    // mem alloc'ed
        size_t offset;

        BinaryPacketState binaryPacketState;
    };

    struct Chunk {
        const char *data;
        unsigned int size;
    };

    typedef void (Receiver::*ChunkProcessor)();
    typedef void (ICallback::*ReceivedPktCallback)(const char *data, unsigned int size);
    typedef void (Receiver::*BinaryPacketProcessor)();

    /************************ data ************************/
    static const ChunkProcessor CHUNK_PROCESSOR[_PT_SIZE];
    static const ReceivedPktCallback RECEIVED_PKT_CALLBACK[_PT_SIZE];
    static const BinaryPacketProcessor BINARY_PACKET_PROCESSOR[_BPS_COUNT];
    static const char* BINARY_PKT_START;
    static size_t BINARY_PKT_START_LEN;
    static const char* TEXT_PKT_FINISH;
    static size_t TEXT_PKT_FINISH_LEN;

    ICallback* _callback;
    PacketDescr _currentPacket;

    /************************ methods ************************/
    /*** misc work with _currentPacket and data ***/
    /**
     * Fetch amount of data available in {\code _currentPacket}
     * \return number of bytes available since offset
     */
    size_t dataAvailable() const;
    /**
     * \return current offset
     */
    size_t dataRead() const;
    /**
     * add amount to offset
     * \return new offset
     */
    size_t dataRead(size_t amount);
    /**
     * \return pointer to currently read data
     */
    const char *getData() const;

    bool ensureDataAvailable(size_t required);

    int32_t lsbToHost(int32_t v);

    /**
     * dismiss currently read data (till offset)
     */
    void shift();
    /**
     * Store another chunk in the end of _currentPacket.data
     */
    void append(const Chunk &chunk);

    /*** packet processors ***/
    void anotherChunkReceived(const Chunk &chunk);
    void anotherChunkReceivedImpl();
    void packetStarted();
    void packetContinueBinary();
    void packetContinueText();

    void packetFinished(size_t packetSize, size_t sizeToRead);

    void binaryPacketHeader();
    void binaryPacketData();

    const char *memmem(const char *haystack, size_t haystackLen,
                       const char *needle, size_t needleLen) const;

public:
    Receiver(ICallback* callback);
    ~Receiver();

    void Receive(const char *data, unsigned int size) override;
};

#endif  /* ISS_RECEIVER_H */
