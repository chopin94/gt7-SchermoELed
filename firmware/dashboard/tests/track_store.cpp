// Tests of src/TrackStore.h on a file system in memory:
//   g++ -std=c++11 -Wall -Wextra -pedantic tests/track_store.cpp -o /tmp/store-tests && /tmp/store-tests
#include "stadium.h"
#include "../src/TrackStore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

using namespace LapAnalysis;
using namespace stadium;

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
    } while (0)

// A file system in memory that lists the oldest file first, like the real one.
struct MemoryStorage : TrackStore::Storage
{
    struct File { std::string path; std::vector<uint8_t> data; };
    std::vector<File> files;
    bool failWrites = false;

    File *find(const char *path)
    {
        for (size_t i = 0; i < files.size(); ++i)
            if (files[i].path == path) return &files[i];
        return nullptr;
    }
    bool read(const char *path, uint8_t *buffer, size_t capacity, size_t &fileSize) override
    {
        File *f = find(path);
        if (!f) return false;
        fileSize = f->data.size();
        memcpy(buffer, f->data.data(), capacity < f->data.size() ? capacity : f->data.size());
        return true;
    }
    bool write(const char *path, const uint8_t *data, size_t length) override
    {
        if (failWrites) return false;
        File *f = find(path);
        if (!f) { files.push_back(File()); f = &files.back(); f->path = path; }
        f->data.assign(data, data + length);
        return true;
    }
    bool remove(const char *path) override
    {
        for (size_t i = 0; i < files.size(); ++i)
            if (files[i].path == path) { files.erase(files.begin() + i); return true; }
        return false;
    }
    void list(bool (*visit)(const char *name, void *context), void *context) override
    {
        const std::vector<File> copy = files; // the visitor may remove files
        for (size_t i = 0; i < copy.size(); ++i)
            if (copy[i].path.compare(0, 3, "/t/") == 0 && !visit(copy[i].path.c_str() + 3, context)) return;
    }
    size_t count(const char *suffix) const
    {
        size_t n = 0;
        for (size_t i = 0; i < files.size(); ++i)
        {
            const std::string &p = files[i].path;
            const size_t s = strlen(suffix);
            if (p.size() >= s && p.compare(p.size() - s, s, suffix) == 0) n++;
        }
        return n;
    }
};

// A session on a circuit (moved sideways by offsetX) that fills the store.
static uint32_t drive(TrackStore::Store &store, float offsetX, int32_t car, bool fraction = false)
{
    Analyzer &a = *new Analyzer();
    Driver d(a);
    d.offsetX = offsetX;
    d.fractionMode = fraction;
    d.s = 1400;
    d.driveToLap(2);
    store.service(a, car);
    const uint32_t key = a.trackKey();
    delete &a;
    return key;
}

static void testSaveAndRestore()
{
    MemoryStorage fs;
    TrackStore::Store store(fs);
    store.begin();
    CHECK(store.trackCount() == 0);
    const uint32_t key = drive(store, 0, 1234);
    CHECK(key != 0);
    CHECK(fs.count(".m") == 1 && fs.count(".r") == 1);
    CHECK(store.trackCount() == 1);

    // Another day: a new store reads the index from the files.
    TrackStore::Store later(fs);
    later.begin();
    CHECK(later.trackCount() == 1);

    Analyzer &a = *new Analyzer();
    Driver d(a);
    d.s = 1400;
    d.drive(40); // on the second corner, going the right way
    CHECK(a.wantsTrack());
    CHECK(later.restoreTrack(a));
    CHECK(a.trackKey() == key);
    CHECK(!a.wantsTrack());
    CHECK(!later.restoreTrack(a)); // already loaded
    CHECK(later.restoreReference(a, 1234));
    CHECK(a.referenceLapMs() > 0 && a.referenceBrakeZoneCount() == 2);
    // Nothing saved for another car.
    Analyzer &b = *new Analyzer();
    Driver e(b);
    e.s = 1400;
    e.drive(40);
    CHECK(later.restoreTrack(b));
    CHECK(!later.restoreReference(b, 999));
    CHECK(b.referenceLapMs() < 0);
    delete &a;
    delete &b;
}

static void testWrongPlaceOrDirection()
{
    MemoryStorage fs;
    TrackStore::Store store(fs);
    store.begin();
    drive(store, 0, 1);

    {   // Far from the saved circuit.
        Analyzer &a = *new Analyzer();
        Driver d(a);
        d.offsetX = 8000;
        d.s = 1400;
        d.drive(40);
        CHECK(!store.restoreTrack(a));
        CHECK(a.wantsTrack());
        delete &a;
    }
    {   // The same road backwards: another circuit.
        Analyzer &a = *new Analyzer();
        Driver d(a);
        d.s = 400; // top straight
        d.drive(20);
        CHECK(store.restoreTrack(a)); // forwards: found
        delete &a;
        // Backwards: feed a car heading -x on the top straight.
        Analyzer &b = *new Analyzer();
        for (int i = 0; i < 40; ++i)
        {
            Sample s;
            s.timeMs = 1000 + i * 17;
            s.speed = 30; s.hasPosition = true; s.x = 300 - i * 0.5f; s.z = 0;
            s.hasVelocity = true; s.vx = -30; s.vz = 0;
            s.lapTimeMs = i * 17;
            b.update(s);
        }
        CHECK(!store.restoreTrack(b));
        delete &b;
    }
    {   // Standing still or without a position: nothing to recognise.
        Analyzer &a = *new Analyzer();
        CHECK(!store.restoreTrack(a));
        delete &a;
    }
}

static void testEraseAndLimits()
{
    MemoryStorage fs;
    TrackStore::Store store(fs);
    store.begin();
    const uint32_t first = drive(store, 0, 1);
    drive(store, 0, 2); // second car on the same circuit
    drive(store, 5000, 1);
    CHECK(store.trackCount() == 2);
    CHECK(fs.count(".m") == 2 && fs.count(".r") == 3);

    store.eraseTrack(first);
    CHECK(store.trackCount() == 1);
    CHECK(fs.count(".m") == 1 && fs.count(".r") == 1);

    store.eraseAll();
    CHECK(store.trackCount() == 0 && fs.files.empty());

    // More circuits than the limit: the oldest are replaced.
    for (int i = 0; i < TrackStore::MAX_TRACKS + 3; ++i) drive(store, 10000.0f * (i + 1), 7);
    CHECK(store.trackCount() == TrackStore::MAX_TRACKS);
    CHECK(fs.count(".m") == static_cast<size_t>(TrackStore::MAX_TRACKS));
    CHECK(fs.count(".r") == static_cast<size_t>(TrackStore::MAX_TRACKS)); // laps go with their circuit
    // The newest is still there, the first is gone.
    Analyzer &a = *new Analyzer();
    Driver d(a);
    d.offsetX = 10000.0f * (TrackStore::MAX_TRACKS + 3);
    d.s = 1400; d.drive(40);
    CHECK(store.restoreTrack(a));
    delete &a;
    Analyzer &b = *new Analyzer();
    Driver e(b);
    e.offsetX = 10000.0f;
    e.s = 1400; e.drive(40);
    CHECK(!store.restoreTrack(b));
    delete &b;
}

static void testTooManyReferenceLaps()
{
    MemoryStorage fs;
    TrackStore::Store store(fs);
    store.begin();
    // One circuit, many cars.
    for (int car = 1; car <= TrackStore::MAX_REFERENCES + 5; ++car) drive(store, 0, car);
    CHECK(fs.count(".m") == 1);
    CHECK(fs.count(".r") == static_cast<size_t>(TrackStore::MAX_REFERENCES));
}

static void testDamagedAndFullStorage()
{
    MemoryStorage fs;
    TrackStore::Store store(fs);
    store.begin();
    drive(store, 0, 1);
    // A cut write: the circuit file is damaged.
    for (size_t i = 0; i < fs.files.size(); ++i)
        if (fs.files[i].path.size() > 2 && fs.files[i].path.compare(fs.files[i].path.size() - 2, 2, ".m") == 0)
            fs.files[i].data[100] ^= 0xFF;
    Analyzer &a = *new Analyzer();
    Driver d(a);
    d.s = 1400; d.drive(40);
    CHECK(!store.restoreTrack(a));
    CHECK(a.wantsTrack());
    delete &a;
    // A short file is ignored by the index.
    MemoryStorage other;
    other.write("/t/00000001.m", reinterpret_cast<const uint8_t *>("short"), 5);
    other.write("/t/readme.txt", reinterpret_cast<const uint8_t *>("x"), 1);
    TrackStore::Store indexed(other);
    indexed.begin();
    CHECK(indexed.trackCount() == 0);

    // The file system is full: nothing is saved, nothing breaks.
    MemoryStorage full;
    full.failWrites = true;
    TrackStore::Store brimming(full);
    brimming.begin();
    drive(brimming, 0, 1);
    CHECK(full.files.empty() && brimming.trackCount() == 0);
}

static void testFractionSourceKeepsItsOwn()
{
    MemoryStorage fs;
    TrackStore::Store store(fs);
    store.begin();
    drive(store, 0, 0, true); // Assetto Corsa
    CHECK(store.trackCount() == 1);
    // A GT7 session on the same road does not load the AC circuit.
    Analyzer &a = *new Analyzer();
    Driver d(a);
    d.s = 1400; d.drive(40);
    CHECK(!store.restoreTrack(a));
    delete &a;
    Analyzer &b = *new Analyzer();
    Driver e(b);
    e.fractionMode = true;
    e.s = 1400; e.drive(40);
    CHECK(store.restoreTrack(b));
    delete &b;
}

int main()
{
    testSaveAndRestore();
    testWrongPlaceOrDirection();
    testEraseAndLimits();
    testTooManyReferenceLaps();
    testDamagedAndFullStorage();
    testFractionSourceKeepsItsOwn();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    printf("track store: all checks passed\n");
    return 0;
}
