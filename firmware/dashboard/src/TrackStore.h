#pragma once
#include "LapAnalysis.h"
#include <new>

// Keeps the circuits the lap analysis has learnt, and the best lap of each
// car on them, in a small file system, so the next session starts with the
// map, the delta, the sectors and the braking points already in place.
//
// Plain C++11 on top of an abstract Storage: LittleFS on the ESP32 (see
// main.cpp), a map in memory in the tests (tests/track_store.cpp).
//
// Files, in the folder /t:
//   <key>.m        the circuit: map and lap length (LapAnalysis::exportTrack)
//   <key>-<car>.r  the best lap of a car on it (LapAnalysis::exportReference)
// <key> is the circuit's key and <car> the car's code, both eight hex digits.
namespace TrackStore
{
static constexpr int MAX_TRACKS = 16;        // beyond these the oldest are replaced
static constexpr int MAX_REFERENCES = 48;
// The start of a circuit file holds the bounds of the map: enough to know
// whether the car can be on it without reading the whole file.
static constexpr size_t TRACK_HEADER_BYTES = 4 + 1 + 1 + 2 + 2 + 4 + 4 * 4;

class Storage
{
public:
    virtual ~Storage() {}
    // Reads up to `capacity` bytes of a file. fileSize is the whole file.
    virtual bool read(const char *path, uint8_t *buffer, size_t capacity, size_t &fileSize) = 0;
    virtual bool write(const char *path, const uint8_t *data, size_t length) = 0;
    virtual bool remove(const char *path) = 0;
    // Names (without the folder) of the files in /t, the oldest first.
    // visit returns false to stop.
    virtual void list(bool (*visit)(const char *name, void *context), void *context) = 0;
};

class Store
{
public:
    explicit Store(Storage &s) : storage(s) {}

    // Reads which circuits are saved. Call once at start.
    void begin()
    {
        trackCount_ = 0;
        storage.list(collectTrack, this);
    }
    int trackCount() const { return trackCount_; }

    // While no circuit is known: is the car on a saved one, going the same
    // way? Then it is loaded. Cheap when the car is far from every circuit
    // (the bounds are in memory), a 2 KB read when it is on one.
    bool restoreTrack(LapAnalysis::Analyzer &analyzer)
    {
        float x, z, hx, hz;
        if (!analyzer.wantsTrack() || !analyzer.pose(x, z, hx, hz)) return false;
        for (int i = 0; i < trackCount_; ++i)
        {
            const Entry &e = tracks[i];
            const float margin = LapAnalysis::TRACK_MATCH_M;
            if (x < e.minX - margin || x > e.maxX + margin || z < e.minZ - margin || z > e.maxZ + margin) continue;
            uint8_t *buffer = new (std::nothrow) uint8_t[LapAnalysis::TRACK_BLOB_SIZE];
            if (!buffer) return false;
            char path[32];
            trackPath(path, sizeof(path), e.key);
            size_t size = 0;
            bool loaded = false;
            if (storage.read(path, buffer, LapAnalysis::TRACK_BLOB_SIZE, size) && size == LapAnalysis::TRACK_BLOB_SIZE &&
                LapAnalysis::Analyzer::trackBlobMatches(buffer, size, x, z, hx, hz))
                loaded = analyzer.importTrack(buffer, size);
            delete[] buffer;
            if (loaded) return true;
        }
        return false;
    }

    // The best lap of this car on the circuit that is loaded, if saved.
    bool restoreReference(LapAnalysis::Analyzer &analyzer, int32_t carCode)
    {
        if (analyzer.trackKey() == 0) return false;
        uint8_t *buffer = new (std::nothrow) uint8_t[LapAnalysis::REFERENCE_BLOB_MAX];
        if (!buffer) return false;
        char path[32];
        referencePath(path, sizeof(path), analyzer.trackKey(), carCode);
        size_t size = 0;
        bool loaded = false;
        if (storage.read(path, buffer, LapAnalysis::REFERENCE_BLOB_MAX, size) && size <= LapAnalysis::REFERENCE_BLOB_MAX)
            loaded = analyzer.importReference(buffer, size);
        delete[] buffer;
        return loaded;
    }

    // Writes what the analyzer has asked to save. Call after each update.
    void service(LapAnalysis::Analyzer &analyzer, int32_t carCode)
    {
        LapAnalysis::SaveKind kind;
        while (analyzer.takeSaveRequest(kind))
        {
            const size_t capacity = kind == LapAnalysis::SaveKind::Track ? LapAnalysis::TRACK_BLOB_SIZE
                                                                         : LapAnalysis::REFERENCE_BLOB_MAX;
            uint8_t *buffer = new (std::nothrow) uint8_t[capacity];
            if (!buffer) continue;
            size_t length = 0;
            char path[32];
            if (kind == LapAnalysis::SaveKind::Track)
            {
                if (analyzer.exportTrack(buffer, capacity, length))
                {
                    trackPath(path, sizeof(path), analyzer.trackKey());
                    if (storage.write(path, buffer, length)) rememberTrack(analyzer.trackKey(), buffer);
                }
            }
            else if (analyzer.exportReference(buffer, capacity, length))
            {
                referencePath(path, sizeof(path), analyzer.trackKey(), carCode);
                storage.write(path, buffer, length);
                limitReferences();
            }
            delete[] buffer;
        }
    }

    // A circuit and the laps saved on it.
    void eraseTrack(uint32_t key)
    {
        if (key == 0) return;
        char path[32];
        trackPath(path, sizeof(path), key);
        storage.remove(path);
        for (int i = 0; i < trackCount_; ++i)
            if (tracks[i].key == key) { removeEntry(i); break; }
        EraseContext context = {this, key};
        storage.list(eraseReferenceOf, &context);
    }

    // Every saved circuit and lap, also files the index does not know.
    void eraseAll()
    {
        storage.list(removeAny, this);
        trackCount_ = 0;
    }

private:
    struct Entry
    {
        uint32_t key;
        float minX, minZ, maxX, maxZ;
    };
    Storage &storage;
    Entry tracks[MAX_TRACKS];
    int trackCount_ = 0;

    struct EraseContext
    {
        Store *store;
        uint32_t key;
    };

    static unsigned long asLong(uint32_t v) { return static_cast<unsigned long>(v); }
    static void trackPath(char *out, size_t size, uint32_t key)
    {
        snprintf(out, size, "/t/%08lx.m", asLong(key));
    }
    static void referencePath(char *out, size_t size, uint32_t key, int32_t car)
    {
        snprintf(out, size, "/t/%08lx-%08lx.r", asLong(key), asLong(static_cast<uint32_t>(car)));
    }

    static bool endsWith(const char *name, const char *suffix)
    {
        const size_t n = strlen(name), s = strlen(suffix);
        return n >= s && strcmp(name + n - s, suffix) == 0;
    }
    static bool parseKey(const char *name, uint32_t &key)
    {
        if (strlen(name) < 8) return false;
        char digits[9];
        memcpy(digits, name, 8);
        digits[8] = 0;
        char *end = nullptr;
        const unsigned long value = strtoul(digits, &end, 16);
        if (!end || *end != 0) return false;
        key = static_cast<uint32_t>(value);
        return true;
    }

    // Reads the bounds of a saved circuit into the index.
    static bool collectTrack(const char *name, void *context)
    {
        Store &self = *static_cast<Store *>(context);
        uint32_t key;
        if (!endsWith(name, ".m") || strlen(name) != 10 || !parseKey(name, key)) return true;
        if (self.trackCount_ >= MAX_TRACKS) return true;
        uint8_t header[TRACK_HEADER_BYTES];
        char path[32];
        trackPath(path, sizeof(path), key);
        size_t size = 0;
        if (!self.storage.read(path, header, sizeof(header), size) || size != LapAnalysis::TRACK_BLOB_SIZE) return true;
        self.addEntry(key, header);
        return true;
    }

    void addEntry(uint32_t key, const uint8_t *header)
    {
        Entry &e = tracks[trackCount_];
        e.key = key;
        float bounds[4];
        memcpy(bounds, header + TRACK_HEADER_BYTES - sizeof(bounds), sizeof(bounds));
        e.minX = bounds[0]; e.minZ = bounds[1]; e.maxX = bounds[2]; e.maxZ = bounds[3];
        trackCount_++;
    }

    void removeEntry(int index)
    {
        for (int i = index; i + 1 < trackCount_; ++i) tracks[i] = tracks[i + 1];
        trackCount_--;
    }

    // A circuit was written: it is in the index, and the oldest make room.
    void rememberTrack(uint32_t key, const uint8_t *blob)
    {
        for (int i = 0; i < trackCount_; ++i)
            if (tracks[i].key == key) removeEntry(i);
        while (trackCount_ >= MAX_TRACKS) eraseTrack(tracks[0].key);
        addEntry(key, blob);
    }

    static bool removeAny(const char *name, void *context)
    {
        char path[40];
        snprintf(path, sizeof(path), "/t/%s", name);
        static_cast<Store *>(context)->storage.remove(path);
        return true;
    }

    static bool eraseReferenceOf(const char *name, void *context)
    {
        EraseContext &c = *static_cast<EraseContext *>(context);
        uint32_t key;
        if (!endsWith(name, ".r") || !parseKey(name, key)) return true;
        if (key == c.key)
        {
            char path[32];
            snprintf(path, sizeof(path), "/t/%s", name);
            c.store->storage.remove(path);
        }
        return true;
    }

    struct CountContext
    {
        int count;
        char oldest[24];
    };
    static bool countReference(const char *name, void *context)
    {
        CountContext &c = *static_cast<CountContext *>(context);
        if (!endsWith(name, ".r") || strlen(name) >= sizeof(c.oldest)) return true;
        if (c.count == 0) snprintf(c.oldest, sizeof(c.oldest), "%s", name);
        c.count++;
        return true;
    }
    // At most MAX_REFERENCES laps are kept: the oldest go first.
    void limitReferences()
    {
        for (int guard = 0; guard < MAX_REFERENCES; ++guard)
        {
            CountContext c = {0, {0}};
            storage.list(countReference, &c);
            if (c.count <= MAX_REFERENCES) return;
            char path[40];
            snprintf(path, sizeof(path), "/t/%s", c.oldest);
            storage.remove(path);
        }
    }
};
} // namespace TrackStore
