#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "iface.h"
#include "receiver.h"

static const size_t ITER_COUNT = 5001;
static const size_t MIN_SIZE = 1; //1024;
static const size_t MAX_SIZE = 1024 * 8;

#define PROB_F 10000
static const int ZERO_SIZE_PROB = 33;
static const int TEXT_PKT_PROB = 50;
#define TEXT_FINISH "\r\n\r\n"
#define BIN_START "\x24"

struct Callback : public ICallback {
    int8_t sum;
    size_t count;

    Callback();
    void BinaryPacket(const char * data, unsigned int size) override;
    void TextPacket(const char * data, unsigned int size) override;
};

Callback::Callback()
: sum(0), count(0)
{
}

void Callback::BinaryPacket(const char* data, unsigned int size)
{
    for (const char *p = data; p < data + size; ++p) {
        sum ^= *p;
    }

    count += size;
}

void Callback::TextPacket(const char* data, unsigned int size)
{
    for (const char *p = data; p < data + size; ++p) {
        sum ^= *p;
    }

    count += size;
}

static bool isZeroSize() {
    return (rand() % (100 * PROB_F)) < (ZERO_SIZE_PROB * PROB_F);
}

static bool isTextPkt() {
    return (rand() % (100 * PROB_F)) < (TEXT_PKT_PROB * PROB_F);
}

static size_t getSize() {
    return (rand() % (MAX_SIZE - MIN_SIZE)) + MIN_SIZE;
}


static ssize_t readSome(int fd, char *data, size_t required, bool isTxt, int8_t *sum, size_t *count) {
    char *buf = isTxt ? data : (data + strlen(BIN_START) + sizeof(uint32_t));
    ssize_t bytesRead = read(fd, buf, required);

    if (bytesRead < 0) {
        perror("Couldn't read data");
        abort();
    }

    for (const char *p = buf; p < buf + bytesRead; ++p) {
        *sum ^= *p;
    }

    *count += bytesRead;

    if (isTxt) {
        memcpy(buf + bytesRead, TEXT_FINISH, strlen(TEXT_FINISH));
    } else {
        memcpy(data, BIN_START, strlen(BIN_START));
        uint32_t *size = reinterpret_cast<uint32_t *>(data + strlen(BIN_START));
        *size = static_cast<uint32_t>(bytesRead & 0xffffffff);
    }

    return bytesRead;
}

static void sendInMultipleIterations(IReceiver *rcv, const char *data, size_t size) {
    size_t dataSent = 0;

    do {
        size_t dataToSend = (rand() % (size - dataSent)) + 1;

        rcv->Receive(data + dataSent, dataToSend);

        dataSent += dataToSend;
    } while (dataSent < size);
}

int main(int argc, char **argv) {
    Callback cb;
    Receiver rcv(&cb);

    int8_t sum = 0;
    size_t count = 0;

    struct stat st;
    const char *filename = argv[1];

    srand(time(nullptr));

    if (stat(filename, &st) < 0) {
        perror("Can't stat on input file");
        abort();
    }

    for (size_t idx = 0; idx < ITER_COUNT; ++idx) {
        int fd = open(filename, 0, O_RDONLY);
        if (fd < 0) {
            perror("Can't open input file");
            abort();
        }

        ssize_t dataRead;
        size_t size;

        do {
            bool text = isTextPkt();
            size = isZeroSize() ? 0 : getSize();
            char *data = nullptr;
            size_t sizeToSend = size;

            if (text) {
                sizeToSend += strlen(TEXT_FINISH);
            } else {
                sizeToSend += strlen(BIN_START) + sizeof(uint32_t);
            }

            data = reinterpret_cast<char *>(malloc(sizeToSend));
            dataRead = readSome(fd, data, size, text, &sum, &count);
            sizeToSend -= (size - dataRead);
            sendInMultipleIterations(&rcv, data, sizeToSend);

            free(data);
        } while(dataRead > 0 || size == 0);

        close(fd);
    }

    printf("Sum: %x vs %x\n", static_cast<unsigned int>(sum), static_cast<unsigned int>(cb.sum));
    printf("Count: %zd vs %zd\n", count, cb.count);

    return 0;
}
