#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <vector>
#include "TrackStore.h"

// Storage of the saved circuits on the LittleFS partition of the flash (the
// "spiffs" partition of partitions.csv). If it cannot be mounted the screen
// simply works without saving anything.
class LittleFsStorage : public TrackStore::Storage
{
public:
    bool begin()
    {
        // The first start formats the partition (a couple of seconds).
        ready = LittleFS.begin(true);
        if (ready && !LittleFS.exists("/t")) LittleFS.mkdir("/t");
        return ready;
    }
    bool available() const { return ready; }

    bool read(const char *path, uint8_t *buffer, size_t capacity, size_t &fileSize) override
    {
        if (!ready) return false;
        File file = LittleFS.open(path, "r");
        if (!file || file.isDirectory()) return false;
        fileSize = file.size();
        const size_t wanted = capacity < fileSize ? capacity : fileSize;
        const bool ok = file.read(buffer, wanted) == wanted;
        file.close();
        return ok;
    }

    bool write(const char *path, const uint8_t *data, size_t length) override
    {
        if (!ready) return false;
        File file = LittleFS.open(path, "w");
        if (!file) return false;
        const size_t written = file.write(data, length);
        file.close();
        if (written != length)
        {
            // Full: a partial file would only be rejected at the next load.
            LittleFS.remove(path);
            return false;
        }
        return true;
    }

    bool remove(const char *path) override { return ready && LittleFS.remove(path); }

    void list(bool (*visit)(const char *name, void *context), void *context) override
    {
        if (!ready) return;
        // The names are collected first: the visitor may delete files.
        std::vector<String> names;
        {
            File folder = LittleFS.open("/t");
            if (!folder || !folder.isDirectory()) return;
            for (File file = folder.openNextFile(); file && names.size() < 160; file = folder.openNextFile())
                if (!file.isDirectory()) names.push_back(String(file.name()));
        }
        for (size_t i = 0; i < names.size(); ++i)
            if (!visit(names[i].c_str(), context)) return;
    }

private:
    bool ready = false;
};
