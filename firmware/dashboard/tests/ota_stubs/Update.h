// Host stand-in for the Update library: keeps the written image in memory.
#pragma once
#include <Arduino.h>
#include <vector>

#define UPDATE_ERROR_OK                 (0)
#define UPDATE_ERROR_WRITE              (1)
#define UPDATE_ERROR_ERASE              (2)
#define UPDATE_ERROR_READ               (3)
#define UPDATE_ERROR_SPACE              (4)
#define UPDATE_ERROR_SIZE               (5)
#define UPDATE_ERROR_STREAM             (6)
#define UPDATE_ERROR_MD5                (7)
#define UPDATE_ERROR_MAGIC_BYTE         (8)
#define UPDATE_ERROR_ACTIVATE           (9)
#define UPDATE_ERROR_NO_PARTITION       (10)
#define UPDATE_ERROR_BAD_ARGUMENT       (11)
#define UPDATE_ERROR_ABORT              (12)
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

class UpdateClass {
public:
    bool begin(size_t size = UPDATE_SIZE_UNKNOWN)
    {
        if (running) return false;
        error = UPDATE_ERROR_OK;
        if (size == UPDATE_SIZE_UNKNOWN) size = partitionSize;
        if (size > partitionSize) { error = UPDATE_ERROR_SIZE; return false; }
        running = true;
        expected = size;
        image.clear();
        ++begins;
        return true;
    }
    size_t write(uint8_t *data, size_t len)
    {
        if (!running || error) return 0;
        if (image.empty() && data[0] != 0xE9) { error = UPDATE_ERROR_MAGIC_BYTE; running = false; return 0; }
        if (image.size() + len > expected) { error = UPDATE_ERROR_SPACE; running = false; return 0; }
        if (failWriteAt && image.size() + len > failWriteAt) { error = UPDATE_ERROR_WRITE; running = false; return 0; }
        image.insert(image.end(), data, data + len);
        return len;
    }
    bool end(bool evenIfRemaining = false)
    {
        endedEvenIfRemaining = evenIfRemaining;
        if (!running || error) return false;
        if (!evenIfRemaining && image.size() != expected) { error = UPDATE_ERROR_ABORT; running = false; return false; }
        running = false;
        if (failVerify) { error = UPDATE_ERROR_ACTIVATE; return false; }
        bootSet = true;
        return true;
    }
    void abort() { running = false; error = UPDATE_ERROR_ABORT; ++aborts; }
    bool isRunning() { return running; }
    uint8_t getError() { return error; }
    const char *errorString() { return "errore"; }

    // Test controls and observations.
    size_t partitionSize = 0x1F0000;
    size_t failWriteAt = 0;
    bool failVerify = false;
    bool running = false;
    bool bootSet = false;
    bool endedEvenIfRemaining = false;
    int begins = 0, aborts = 0;
    uint8_t error = UPDATE_ERROR_OK;
    size_t expected = 0;
    std::vector<uint8_t> image;
};
extern UpdateClass Update;

struct HostEsp {
    int restarts = 0;
    void restart() { ++restarts; }
};
extern HostEsp ESP;
