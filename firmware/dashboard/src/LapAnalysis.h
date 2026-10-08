#pragma once
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>

// Lap analysis computed on the dashboard from the raw telemetry of GT7 and
// Assetto Corsa: live delta against the best lap, sector times, a track map
// recorded while driving, G-forces, pedal traces, acceleration runs and the
// session summary. Plain C++ without Arduino types, so the same code runs on
// the ESP32, in the unit tests (tests/lap_analysis.cpp) and in the screenshot
// renderer (tools/screenshots).
//
// Coordinates: x and z are the horizontal world axes in metres. Both games are
// drawn with x to the right and z downwards, which matches the in-game maps.
namespace LapAnalysis
{
// Live delta: the lap time is stored every DISTANCE_CELL_M metres along the lap
// (GT7) or every 1/DELTA_CELLS of the lap (Assetto Corsa reports the fraction).
static constexpr int DELTA_CELLS = 2048;
static constexpr float DISTANCE_CELL_M = 12.5f; // 2048 cells cover 25.6 km

// Track map: raw points every few metres during the first lap, then the lap is
// resampled to MAP_POINTS points evenly spaced along its length.
static constexpr int MAP_RAW_POINTS = 512;
static constexpr int MAP_POINTS = 256;
static constexpr float MAP_FIRST_SPACING_M = 6.0f;

static constexpr int SECTORS = 3;
static constexpr int SESSION_LAPS = 10;
static constexpr int G_TRAIL = 40;              // 40 x 80 ms: 3.2 s of trail
static constexpr uint32_t G_TRAIL_STEP_MS = 80;
static constexpr int TRACE_SAMPLES = 144;       // 144 x 50 ms: 7.2 s of pedals
static constexpr uint32_t TRACE_STEP_MS = 50;
static constexpr float GRAVITY = 9.80665f;

// Braking points: where the pedal goes down for each corner of the lap. They
// are compared with the ones of the reference (best) lap.
static constexpr int MAX_BRAKE_ZONES = 24;
static constexpr float BRAKE_ON = 0.25f;          // pedal that starts a zone
static constexpr float BRAKE_OFF = 0.10f;         // pedal that keeps it going
static constexpr float BRAKE_MIN_SPEED = 20.0f;   // m/s: slower is not a braking zone
static constexpr float BRAKE_MIN_DROP = 1.7f;     // m/s lost within the confirmation time
static constexpr uint32_t BRAKE_CONFIRM_MS = 350;
static constexpr uint32_t BRAKE_RELEASE_MS = 400;
static constexpr uint32_t BRAKE_GAP_MS = 1500;    // a new zone needs this long since the last
static constexpr float BRAKE_MATCH_ALONG_M = 150.0f;
static constexpr float BRAKE_MATCH_SIDE_M = 45.0f;
static constexpr float BRAKE_LOOKAHEAD_M = 500.0f;

// One telemetry update. Fields a game does not report keep their defaults.
struct Sample
{
    uint32_t timeMs = 0;      // sample clock (GT7 packet clock, millis() for AC)
    float speed = 0;          // m/s
    bool hasPosition = false;
    float x = 0, y = 0, z = 0; // world position in metres (y is up)
    bool hasVelocity = false;
    float vx = 0, vz = 0;     // horizontal world velocity in m/s (GT7)
    int lapCount = 0;         // lap counter of the game
    int totalLaps = 0;        // race length in laps, 0 when unknown
    int32_t lapTimeMs = -1;   // current lap time from the game, -1 if unknown
    int32_t lastLapMs = -1;   // official time of the previous lap, -1 if unknown
    float lapFraction = NAN;  // 0-1 along the lap when the game reports it (AC)
    bool driving = true;      // false while paused, loading or in the menus
    bool inPit = false;
    float throttle = 0;       // 0-1
    float brake = 0;          // 0-1
    float fuelFraction = NAN; // 0-1 of the tank, NAN when unknown or electric
    float fuelLaps = NAN;     // estimated laps of fuel left, NAN when unknown
};

enum class SectorState : uint8_t
{
    None,    // not driven yet (or no timing available)
    Running, // the car is in this sector
    Purple,  // best time of the session
    Green,   // faster than the same sector of the best lap
    Yellow,  // slower
};

enum class MapState : uint8_t
{
    Waiting,   // the map starts at the next crossing of the line
    Recording, // first lap in progress, the map is drawn as it goes
    Complete,
};

enum class EventType : uint8_t
{
    BestLap,  // lapMs = new best, gainMs = improvement (-1 for the first lap)
    FinalLap, // the last lap of the race has started
    LowFuel,  // fuel reserve
};

struct Event
{
    EventType type = EventType::BestLap;
    int32_t lapMs = -1;
    int32_t gainMs = -1;
};

struct LapRecord
{
    int32_t lapMs = -1;
    int32_t sectorMs[SECTORS] = {-1, -1, -1};
    float topSpeedKmh = 0;
};

// Where the braking for a corner started.
struct BrakeZone
{
    float x = 0, z = 0;     // position of the car in metres
    float hx = 0, hz = 0;   // direction of travel (unit vector)
    float entryKmh = 0;     // speed when the pedal went down
    float peak = 0;         // strongest pedal, 0-1
};

// How a persisted block is used: see exportTrack and exportReference.
enum class SaveKind : uint8_t
{
    Track,     // circuit map
    Reference, // best lap of the car on the circuit: times, sectors, braking points
};

// Drag-race "Christmas tree" shown by the performance theme.
enum class TreeLight : uint8_t
{
    Off,        // moving: no run being prepared
    Staged,     // stopped and ready
    Amber1,
    Amber2,
    Amber3,
    Green,
    FalseStart, // left before the green light
};

enum class LaunchPhase : uint8_t
{
    Rolling,  // driving, no standing start in progress
    Staged,   // stopped for a second: the next start is timed
    Running,  // standing start in progress
    Finished, // run over (braked or lifted), results kept until the next stop
};

// Times in seconds and speeds in km/h, NAN when not reached.
struct LaunchRun
{
    float reaction = NAN; // start minus green light (negative: false start)
    float t100 = NAN;     // 0-100 km/h
    float t200 = NAN;     // 0-200 km/h
    float t100to200 = NAN;
    float t400 = NAN;     // 400 m
    float v400Kmh = NAN;  // speed at 400 m
    float vmaxKmh = NAN;
};

// ---- Persisted blocks ----------------------------------------------------------
// The circuit map and the reference lap are saved as two small binary blocks
// (see Analyzer::exportTrack and exportReference). Little endian, a CRC-32 at
// the end: a block cut short by a power loss is rejected, never half loaded.
static constexpr uint32_t TRACK_MAGIC = 0x4D544447;     // "GDTM"
static constexpr uint32_t REFERENCE_MAGIC = 0x52544447; // "GDTR"
static constexpr uint8_t BLOB_VERSION = 1;
static constexpr size_t TRACK_BLOB_SIZE = 4 + 1 + 1 + 2 + 2 + 4 + 4 * 4 + MAP_POINTS * 8 + 4;
static constexpr size_t REFERENCE_BLOB_HEADER = 4 + 1 + 1 + 2 + 4 + 4 + 3 * 4 + 4;
static constexpr size_t BRAKE_ZONE_BLOB_SIZE = 6 * 4;
static constexpr size_t REFERENCE_BLOB_MAX =
    REFERENCE_BLOB_HEADER + MAX_BRAKE_ZONES * BRAKE_ZONE_BLOB_SIZE + DELTA_CELLS * 4 + 4;
// The circuit is recognised when the car is this close to the saved map...
static constexpr float TRACK_MATCH_M = 30.0f;

inline uint32_t crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

namespace blob
{
struct Writer
{
    uint8_t *out;
    size_t pos = 0;
    explicit Writer(uint8_t *buffer) : out(buffer) {}
    void put(const void *data, size_t n) { memcpy(out + pos, data, n); pos += n; }
    void u8(uint8_t v) { put(&v, 1); }
    void u16(uint16_t v) { put(&v, 2); }
    void u32(uint32_t v) { put(&v, 4); }
    void i32(int32_t v) { put(&v, 4); }
    void f32(float v) { put(&v, 4); }
};
struct Reader
{
    const uint8_t *in;
    size_t length, pos = 0;
    bool ok = true;
    Reader(const uint8_t *buffer, size_t size) : in(buffer), length(size) {}
    void get(void *data, size_t n)
    {
        if (pos + n > length) { ok = false; memset(data, 0, n); return; }
        memcpy(data, in + pos, n);
        pos += n;
    }
    uint8_t u8() { uint8_t v; get(&v, 1); return v; }
    uint16_t u16() { uint16_t v; get(&v, 2); return v; }
    uint32_t u32() { uint32_t v; get(&v, 4); return v; }
    int32_t i32() { int32_t v; get(&v, 4); return v; }
    float f32() { float v; get(&v, 4); return v; }
};
// Magic, version and CRC of a block: the data are trusted only after this.
inline bool valid(const uint8_t *data, size_t length, uint32_t magic)
{
    if (!data || length < 12) return false;
    uint32_t stored, found;
    memcpy(&stored, data + length - 4, 4);
    memcpy(&found, data, 4);
    return found == magic && data[4] == BLOB_VERSION && crc32(data, length - 4) == stored;
}
} // namespace blob

class Analyzer
{
public:
    // The lap times and the map points (about 22 KB) live on the heap: the
    // ESP32 has much less room for static data than for allocations.
    Analyzer() : buffers(new (std::nothrow) Buffers) { reset(); }
    ~Analyzer() { delete buffers; }
    Analyzer(const Analyzer &) = delete;
    Analyzer &operator=(const Analyzer &) = delete;

    // False if the buffers could not be allocated: no analysis then.
    bool available() const { return buffers != nullptr; }

    // Forgets everything: map, best lap, sectors, session and runs.
    void reset()
    {
        resetTrack();
        hasLapCounter = false;
        lapCounter = 0;
        officialLastSeen = -1;
        pendingOfficialSamples = 0;
        lastTimeMs = 0;
        hasPrevious = false;
        resetMotion();
        launch = LaunchState();
        lastRun = LaunchRun();
        bestRun = LaunchRun();
        rollingStartMs = 0;
        rollingActive = false;
        fuelArmed = true;
        fuelLowest = NAN;
        eventHead = eventCount = 0;
        saveTrackPending = saveReferencePending = false;
    }

    // Another car on the same circuit: times, references and runs start over,
    // the map stays.
    void resetTiming()
    {
        refLapMs = -1;
        refCells = 0;
        if (!lapFractionMode) lapCells = map.state == MapState::Complete ? lapCells : 0;
        for (int i = 0; i < SECTORS; ++i)
        {
            bestSector[i] = refSector[i] = -1;
            lastLapSector[i] = SectorState::None;
        }
        sessionLaps = 0;
        sessionStored = 0;
        sessionBest = -1;
        sessionTotalMs = 0;
        sessionTopKmh = 0;
        peakLat = peakBrake = peakAccel = 0;
        sessionRev++;
        for (int i = 0; i < SECTORS; ++i) lap.sectorState[i] = SectorState::None;
        liveDeltaValid = false;
        sectorRev++;
        launch = LaunchState();
        lastRun = LaunchRun();
        bestRun = LaunchRun();
        rollingActive = false;
        launchRev++;
        clearBrakeZones(true);
        saveReferencePending = false;
    }

    // Forgets what belongs to the circuit: map, references, sectors and laps.
    void resetTrack()
    {
        refLapMs = -1;
        refCells = 0;
        lapCells = 0;
        for (int i = 0; i < SECTORS; ++i)
        {
            bestSector[i] = refSector[i] = -1;
            lastLapSector[i] = SectorState::None;
        }
        // Field by field: a MapData temporary would need 6 KB of stack.
        map.state = MapState::Waiting;
        map.rawCount = 0;
        map.spacing = MAP_FIRST_SPACING_M;
        map.lastRawDistance = 0;
        map.minX = map.minZ = map.maxX = map.maxZ = 0;
        mapRev++;
        offTrackChecks = 0;
        sessionLaps = 0;
        sessionStored = 0;
        sessionBest = -1;
        sessionTotalMs = 0;
        sessionTopKmh = 0;
        peakLat = peakBrake = peakAccel = 0;
        sessionRev++;
        lap = LapState();
        liveDeltaValid = false;
        liveDelta = 0;
        sectorRev++;
        clearBrakeZones(true);
        trackKeyValue = 0;
        saveTrackPending = saveReferencePending = false;
    }

    void update(const Sample &s)
    {
        if (!buffers) return;
        lastTimeMs = s.timeMs;
        if (!s.driving)
        {
            // Paused or in the menus: nothing moves. The next driving sample
            // restarts the derivatives instead of seeing a long time step.
            motionGap = true;
            braking.active = false;
            return;
        }

        const int32_t dtMs = hasPrevious ? static_cast<int32_t>(s.timeMs - previous.timeMs) : 0;
        if (dtMs < 0) motionGap = true;

        checkContinuity(s);
        updateLapCounter(s, dtMs);
        updateOfficialLapTime(s);
        updateProgress(s);
        updateMap(s);
        checkTrackChange(s);
        updateMotion(s, dtMs);
        updateBrakeZones(s);
        updateLaunch(s, dtMs);
        updateFuel(s);

        const float kmh = s.speed * 3.6f;
        if (kmh > sessionTopKmh && kmh < 600.0f) { sessionTopKmh = kmh; sessionRev++; }
        if (kmh > lap.topKmh && kmh < 600.0f) lap.topKmh = kmh;

        previous = s;
        hasPrevious = true;
        motionGap = false;
    }

    // ---- Live delta --------------------------------------------------------
    bool hasLiveDelta() const { return liveDeltaValid; }
    int32_t liveDeltaMs() const { return liveDelta; }
    int32_t referenceLapMs() const { return refLapMs; }

    // ---- Sectors ------------------------------------------------------------
    bool sectorsAvailable() const { return lapCells > 0; }
    int currentSector() const
    {
        return sectorsAvailable() && lap.progressKnown && !lap.standingStart ? lap.sector : -1;
    }
    SectorState sectorState(int i) const { return valid(i) ? lap.sectorState[i] : SectorState::None; }
    int32_t sectorMs(int i) const { return valid(i) ? lap.sectorMs[i] : -1; }
    SectorState lastLapSectorState(int i) const { return valid(i) ? lastLapSector[i] : SectorState::None; }
    int32_t bestSectorMs(int i) const { return valid(i) ? bestSector[i] : -1; }
    uint32_t sectorRevision() const { return sectorRev; }

    // ---- Track map -----------------------------------------------------------
    MapState mapState() const { return map.state; }
    int mapPointCount() const
    {
        if (!buffers) return 0;
        return map.state == MapState::Complete ? MAP_POINTS : map.rawCount;
    }
    void mapPoint(int i, float &x, float &z) const
    {
        if (map.state == MapState::Complete) { x = buffers->mapX[i]; z = buffers->mapZ[i]; }
        else { x = buffers->rawX[i]; z = buffers->rawZ[i]; }
    }
    // Sector of a point of the completed map (the map starts at the line).
    static int mapPointSector(int i) { return i * SECTORS / MAP_POINTS; }
    bool mapBounds(float &minX, float &minZ, float &maxX, float &maxZ) const
    {
        if (mapPointCount() == 0) return false;
        minX = map.minX; minZ = map.minZ; maxX = map.maxX; maxZ = map.maxZ;
        return true;
    }
    float mapRecordedMeters() const { return map.state == MapState::Recording ? lap.distance : 0.0f; }
    uint32_t mapRevision() const { return mapRev; }
    bool carPosition(float &x, float &z) const
    {
        if (!hasPrevious || !previous.hasPosition) return false;
        x = previous.x; z = previous.z;
        return true;
    }
    // 0-1 along the lap, NAN when unknown.
    float lapProgress() const
    {
        if (!lap.progressKnown || lapCells <= 0) return NAN;
        return clampf(lap.progress / static_cast<float>(lapCells), 0.0f, 1.0f);
    }

    // ---- G-forces and pedals ---------------------------------------------------
    // Acceleration of the car in g: lateral positive towards the right (right
    // turn), longitudinal positive when accelerating.
    float lateralG() const { return latAccel / GRAVITY; }
    float longitudinalG() const { return longAccel / GRAVITY; }
    int gTrailCount() const { return trailCount; }
    // age 0 is the newest point.
    void gTrail(int age, float &lat, float &lon) const
    {
        const int i = (trailHead - 1 - age + 2 * G_TRAIL) % G_TRAIL;
        lat = trailLat[i]; lon = trailLong[i];
    }
    float peakLateralG() const { return peakLat / GRAVITY; }
    float peakBrakingG() const { return peakBrake / GRAVITY; }
    float peakAccelerationG() const { return peakAccel / GRAVITY; }
    int traceCount() const { return traceSamples; }
    uint8_t traceThrottle(int age) const { return traceThr[traceIndex(age)]; }
    uint8_t traceBrake(int age) const { return traceBrk[traceIndex(age)]; }
    uint32_t traceRevision() const { return traceRev; }

    // ---- Acceleration runs -------------------------------------------------------
    LaunchPhase launchPhase() const { return launch.phase; }
    TreeLight treeLight() const
    {
        switch (launch.phase)
        {
        case LaunchPhase::Staged:
        {
            const uint32_t since = lastTimeMs - launch.stagedMs;
            if (since >= TREE_GREEN_MS) return TreeLight::Green;
            if (since >= TREE_AMBER_MS + 2 * TREE_STEP_MS) return TreeLight::Amber3;
            if (since >= TREE_AMBER_MS + TREE_STEP_MS) return TreeLight::Amber2;
            if (since >= TREE_AMBER_MS) return TreeLight::Amber1;
            return TreeLight::Staged;
        }
        case LaunchPhase::Running:
        case LaunchPhase::Finished:
            if (!launch.treeUsed) return TreeLight::Off;
            return launch.falseStart ? TreeLight::FalseStart : TreeLight::Green;
        default:
            return TreeLight::Off;
        }
    }
    // Seconds since the start of the current or last run, NAN without a run.
    float runSeconds() const
    {
        if (launch.phase == LaunchPhase::Running) return (lastTimeMs - launch.startMs) / 1000.0f;
        if (launch.phase == LaunchPhase::Finished) return launch.durationS;
        return NAN;
    }
    float runSpeedKmh() const { return hasPrevious ? previous.speed * 3.6f : 0.0f; }
    // The run in progress, or the last one.
    const LaunchRun &currentRun() const { return launch.phase == LaunchPhase::Running ? launch.run : lastRun; }
    const LaunchRun &bestRuns() const { return bestRun; }
    uint32_t launchRevision() const { return launchRev; }

    // ---- Session -------------------------------------------------------------------
    int lapsCompleted() const { return sessionLaps; }
    int storedLaps() const { return sessionStored; }
    // age 0 is the last completed lap.
    const LapRecord &lapRecord(int age) const
    {
        return sessionList[(sessionHead - 1 - age + 2 * SESSION_LAPS) % SESSION_LAPS];
    }
    int32_t bestLapMs() const { return sessionBest; }
    int32_t averageLapMs() const { return sessionLaps > 0 ? static_cast<int32_t>(sessionTotalMs / sessionLaps) : -1; }
    float topSpeedKmh() const { return sessionTopKmh; }
    uint32_t sessionRevision() const { return sessionRev; }

    // ---- Events ----------------------------------------------------------------------
    bool takeEvent(Event &event)
    {
        if (eventCount == 0) return false;
        event = events[eventHead];
        eventHead = (eventHead + 1) % EVENT_QUEUE;
        eventCount--;
        return true;
    }

    // ---- Braking points -----------------------------------------------------------
    // Braking points of this lap, in the order they were taken.
    int brakeZoneCount() const { return curZoneCount; }
    // Braking points of the reference (best) lap.
    int referenceBrakeZoneCount() const { return buffers ? refZoneCount : 0; }
    bool referenceBrakeZone(int i, BrakeZone &zone) const
    {
        if (!buffers || i < 0 || i >= refZoneCount) return false;
        zone = buffers->refZones[i];
        return true;
    }
    // Metres to the next braking point of the reference lap and the speed it
    // was taken at; false when there is none within BRAKE_LOOKAHEAD_M ahead.
    bool nextBrakePoint(float &meters, float &referenceKmh) const
    {
        if (!buffers || !hasPrevious || !previous.hasPosition || !hasHeading) return false;
        if (!nextRefValid || nextRef >= refZoneCount) return false;
        const BrakeZone &z = buffers->refZones[nextRef];
        const float dx = z.x - previous.x, dz = z.z - previous.z;
        const float ahead = dx * cosf(heading) + dz * sinf(heading);
        const float distance = hypotf(dx, dz);
        if (ahead <= 0.0f || distance > BRAKE_LOOKAHEAD_M) return false;
        meters = distance;
        referenceKmh = z.entryKmh;
        return true;
    }
    int nextBrakeIndex() const { return nextRefValid && nextRef < refZoneCount ? nextRef : -1; }
    // The last braking of this session: metres earlier (+) or later (-) than
    // the reference lap (NAN without a reference), entry speed and the
    // reference's. False until the first braking.
    bool lastBrakeResult(float &deltaMeters, float &entryKmh, float &referenceKmh) const
    {
        if (!isfinite(resultEntryKmh)) return false;
        deltaMeters = resultDelta;
        entryKmh = resultEntryKmh;
        referenceKmh = resultReferenceKmh;
        return true;
    }
    int lastBrakeIndex() const { return resultIndex; }
    // Result per reference braking point: this lap and the previous one.
    // NAN where the pedal was not used (yet).
    float brakeDelta(int i) const { return buffers && i >= 0 && i < refZoneCount ? buffers->curDelta[i] : NAN; }
    float lastLapBrakeDelta(int i) const { return buffers && i >= 0 && i < refZoneCount ? buffers->lastDelta[i] : NAN; }
    // Changes only when the reference braking points change (not with every
    // braking): the map redraws its markers when it does.
    uint32_t referenceBrakeRevision() const { return refZoneRev; }
    uint32_t brakeRevision() const { return brakeRev; }

    // ---- Persistence ---------------------------------------------------------------
    // The circuit is saved after its map is complete and the reference lap
    // every time it improves: takeSaveRequest says what to write, one block
    // at a time. The caller stores the exported bytes and gives them back to
    // importTrack and importReference in a later session.
    uint32_t trackKey() const { return trackKeyValue; }
    bool takeSaveRequest(SaveKind &kind)
    {
        if (trackKeyValue == 0) return false;
        if (saveTrackPending) { saveTrackPending = false; kind = SaveKind::Track; return true; }
        if (saveReferencePending) { saveReferencePending = false; kind = SaveKind::Reference; return true; }
        return false;
    }
    // True while no circuit is known and a saved one could be loaded.
    bool wantsTrack() const { return buffers && map.state == MapState::Waiting && trackKeyValue == 0; }
    // Position and direction of the car, for recognising a saved circuit.
    bool pose(float &x, float &z, float &hx, float &hz) const
    {
        if (!hasPrevious || !previous.hasPosition || !hasHeading || previous.speed < 8.0f) return false;
        x = previous.x; z = previous.z;
        hx = cosf(heading); hz = sinf(heading);
        return true;
    }
    bool fractionMode() const { return lapFractionMode; }

    bool exportTrack(uint8_t *out, size_t capacity, size_t &length) const
    {
        if (!buffers || map.state != MapState::Complete || trackKeyValue == 0 || capacity < TRACK_BLOB_SIZE)
            return false;
        blob::Writer w(out);
        w.u32(TRACK_MAGIC);
        w.u8(BLOB_VERSION);
        w.u8(lapFractionMode ? 1 : 0);
        w.u16(MAP_POINTS);
        w.u16(static_cast<uint16_t>(lapCells));
        w.u32(trackKeyValue);
        w.f32(map.minX); w.f32(map.minZ); w.f32(map.maxX); w.f32(map.maxZ);
        w.put(buffers->mapX, sizeof(float) * MAP_POINTS);
        w.put(buffers->mapZ, sizeof(float) * MAP_POINTS);
        w.u32(crc32(out, w.pos));
        length = w.pos;
        return true;
    }

    // Loads a saved circuit while no map is known. False if the block is
    // damaged or was saved with another kind of source (distance or fraction).
    bool importTrack(const uint8_t *data, size_t length)
    {
        if (!buffers || map.state != MapState::Waiting || length != TRACK_BLOB_SIZE) return false;
        if (!blob::valid(data, length, TRACK_MAGIC)) return false;
        blob::Reader r(data, length);
        r.u32(); r.u8();
        const uint8_t flags = r.u8();
        const uint16_t points = r.u16(), cells = r.u16();
        const uint32_t key = r.u32();
        const float minX = r.f32(), minZ = r.f32(), maxX = r.f32(), maxZ = r.f32();
        if (points != MAP_POINTS || key == 0 || cells > DELTA_CELLS || ((flags & 1) != 0) != lapFractionMode)
            return false;
        if (!isfinite(minX) || !isfinite(minZ) || !isfinite(maxX) || !isfinite(maxZ)) return false;
        r.get(buffers->mapX, sizeof(float) * MAP_POINTS);
        r.get(buffers->mapZ, sizeof(float) * MAP_POINTS);
        if (!r.ok) return false;
        map.minX = minX; map.minZ = minZ; map.maxX = maxX; map.maxZ = maxZ;
        map.state = MapState::Complete;
        map.rawCount = 0;
        if (!lapFractionMode && cells > 0) lapCells = cells;
        trackKeyValue = key;
        offTrackChecks = 0;
        saveTrackPending = false;
        mapRev++;
        sectorRev++;
        return true;
    }

    // Is the car on the circuit of this saved block, going the same way?
    static bool trackBlobMatches(const uint8_t *data, size_t length, float x, float z, float hx, float hz)
    {
        if (length != TRACK_BLOB_SIZE || !blob::valid(data, length, TRACK_MAGIC)) return false;
        const size_t header = 4 + 1 + 1 + 2 + 2 + 4;
        float bounds[4];
        memcpy(bounds, data + header, sizeof(bounds));
        const float margin = TRACK_MATCH_M;
        if (x < bounds[0] - margin || x > bounds[2] + margin || z < bounds[1] - margin || z > bounds[3] + margin)
            return false;
        float mx[MAP_POINTS], mz[MAP_POINTS];
        memcpy(mx, data + header + sizeof(bounds), sizeof(mx));
        memcpy(mz, data + header + sizeof(bounds) + sizeof(mx), sizeof(mz));
        int nearest = -1;
        float best = TRACK_MATCH_M * TRACK_MATCH_M;
        for (int k = 0; k < MAP_POINTS; ++k)
        {
            const float dx = mx[k] - x, dz = mz[k] - z, d = dx * dx + dz * dz;
            if (d < best) { best = d; nearest = k; }
        }
        if (nearest < 0) return false;
        // The same circuit run backwards is another circuit.
        const int before = (nearest + MAP_POINTS - 2) % MAP_POINTS, after = (nearest + 2) % MAP_POINTS;
        const float tx = mx[after] - mx[before], tz = mz[after] - mz[before];
        const float length2 = hypotf(tx, tz);
        return length2 > 0.1f && (tx * hx + tz * hz) / length2 > 0.5f;
    }

    bool exportReference(uint8_t *out, size_t capacity, size_t &length) const
    {
        if (!buffers || refLapMs <= 0 || refCells < 2 || trackKeyValue == 0) return false;
        const size_t zones = static_cast<size_t>(refZoneCount);
        const size_t size = REFERENCE_BLOB_HEADER + zones * BRAKE_ZONE_BLOB_SIZE + sizeof(uint32_t) * refCells + 4;
        if (capacity < size) return false;
        blob::Writer w(out);
        w.u32(REFERENCE_MAGIC);
        w.u8(BLOB_VERSION);
        w.u8(lapFractionMode ? 1 : 0);
        w.u16(static_cast<uint16_t>(refCells));
        w.u32(trackKeyValue);
        w.i32(refLapMs);
        for (int i = 0; i < SECTORS; ++i) w.i32(refSector[i]);
        w.u8(static_cast<uint8_t>(zones));
        w.u8(0); w.u8(0); w.u8(0);
        for (size_t i = 0; i < zones; ++i)
        {
            const BrakeZone &z = buffers->refZones[i];
            w.f32(z.x); w.f32(z.z); w.f32(z.hx); w.f32(z.hz); w.f32(z.entryKmh); w.f32(z.peak);
        }
        w.put(buffers->refTime, sizeof(uint32_t) * refCells);
        w.u32(crc32(out, w.pos));
        length = w.pos;
        return true;
    }

    // Loads the saved reference lap of this car on the known circuit.
    bool importReference(const uint8_t *data, size_t length)
    {
        if (!buffers || trackKeyValue == 0 || length < REFERENCE_BLOB_HEADER + 4) return false;
        if (!blob::valid(data, length, REFERENCE_MAGIC)) return false;
        blob::Reader r(data, length);
        r.u32(); r.u8();
        const uint8_t flags = r.u8();
        const uint16_t cells = r.u16();
        const uint32_t key = r.u32();
        const int32_t lapMs = r.i32();
        int32_t sectors[SECTORS];
        for (int i = 0; i < SECTORS; ++i) sectors[i] = r.i32();
        const uint8_t zones = r.u8();
        r.u8(); r.u8(); r.u8();
        if (key != trackKeyValue || ((flags & 1) != 0) != lapFractionMode) return false;
        if (cells < 2 || cells > DELTA_CELLS || lapMs <= 0 || zones > MAX_BRAKE_ZONES) return false;
        if (length != REFERENCE_BLOB_HEADER + zones * BRAKE_ZONE_BLOB_SIZE + sizeof(uint32_t) * cells + 4) return false;
        // The lap length must agree with the map (a cell or two of difference
        // is the normal spread between laps).
        if (!lapFractionMode && lapCells > 0 && abs(static_cast<int>(cells) - lapCells) > lapCells / 20 + 2) return false;
        BrakeZone loaded[MAX_BRAKE_ZONES];
        for (int i = 0; i < zones; ++i)
        {
            BrakeZone &z = loaded[i];
            z.x = r.f32(); z.z = r.f32(); z.hx = r.f32(); z.hz = r.f32(); z.entryKmh = r.f32(); z.peak = r.f32();
        }
        r.get(buffers->refTime, sizeof(uint32_t) * cells);
        if (!r.ok) return false;
        refLapMs = lapMs;
        refCells = cells;
        if (!lapFractionMode) lapCells = cells;
        for (int i = 0; i < SECTORS; ++i) refSector[i] = sectors[i];
        for (int i = 0; i < zones; ++i) buffers->refZones[i] = loaded[i];
        refZoneCount = zones;
        for (int i = 0; i < MAX_BRAKE_ZONES; ++i) buffers->curDelta[i] = buffers->lastDelta[i] = NAN;
        nextRef = 0;
        nextRefValid = false;
        saveReferencePending = false;
        brakeRev++;
        refZoneRev++;
        sectorRev++;
        return true;
    }

private:
    static constexpr uint32_t STAGE_MS = 1000;      // stopped this long to stage
    static constexpr uint32_t TREE_AMBER_MS = 1000; // first amber after staging
    static constexpr uint32_t TREE_STEP_MS = 500;   // sportsman tree: one amber every 0.5 s
    static constexpr uint32_t TREE_GREEN_MS = TREE_AMBER_MS + 3 * TREE_STEP_MS;
    static constexpr float STOPPED_MS = 0.5f;       // m/s
    static constexpr float JUMP_M = 60.0f;          // teleport, rewind or new session
    static constexpr int OFFICIAL_WAIT_SAMPLES = 180;
    static constexpr int EVENT_QUEUE = 4;

    struct LapState
    {
        bool observed = false;      // the start of this lap was seen
        bool standingStart = false; // began from the grid, behind the line
        bool progressKnown = false; // lap progress is known (distance or fraction)
        bool continuous = true;     // no rewind or jump since the start
        float distance = 0;         // metres since the line
        float progress = 0;         // cells along the lap
        int32_t lastTime = -1;      // lap time of the last sample (ms)
        float lastProgress = 0;
        int maxCell = -1;           // last delta cell written
        int sector = 0;
        int32_t sectorStart[SECTORS] = {0, -1, -1}; // lap time at each sector start
        int32_t sectorMs[SECTORS] = {-1, -1, -1};
        SectorState sectorState[SECTORS] = {SectorState::None, SectorState::None, SectorState::None};
        float topKmh = 0;
        uint32_t startClockMs = 0;  // sample clock at the line
    };

    struct MapData
    {
        MapState state = MapState::Waiting;
        int rawCount = 0;
        float spacing = MAP_FIRST_SPACING_M;
        float lastRawDistance = 0;
        float minX = 0, minZ = 0, maxX = 0, maxZ = 0;
    };

    struct Buffers
    {
        uint32_t curTime[DELTA_CELLS]; // lap time at each cell of this lap
        uint32_t refTime[DELTA_CELLS]; // the same for the reference lap
        float rawX[MAP_RAW_POINTS];    // map being recorded
        float rawZ[MAP_RAW_POINTS];
        float mapX[MAP_POINTS];        // completed map
        float mapZ[MAP_POINTS];
        BrakeZone curZones[MAX_BRAKE_ZONES]; // braking points of this lap
        BrakeZone refZones[MAX_BRAKE_ZONES]; // of the reference lap
        float curDelta[MAX_BRAKE_ZONES];     // per reference zone, this lap (NAN: not braked)
        float lastDelta[MAX_BRAKE_ZONES];    // the same for the previous lap
    };
    Buffers *buffers;

    struct LaunchState
    {
        LaunchPhase phase = LaunchPhase::Rolling;
        uint32_t stoppedSinceMs = 0;
        bool stopped = false;
        uint32_t stagedMs = 0;
        uint32_t startMs = 0;
        bool falseStart = false;
        bool treeUsed = false;
        float distance = 0;
        float durationS = NAN;
        LaunchRun run;
    };

    // ---- state ----
    Sample previous;
    bool hasPrevious = false;
    bool motionGap = true;
    uint32_t lastTimeMs = 0;

    bool hasLapCounter = false;
    int lapCounter = 0;
    int32_t officialLastSeen = -1;
    int pendingOfficialSamples = 0;
    int32_t pendingMeasuredMs = -1;
    LapState lap;

    int32_t refLapMs = -1;
    int refCells = 0;
    int lapCells = 0; // lap length in delta cells, 0 while unknown
    bool liveDeltaValid = false;
    int32_t liveDelta = 0;

    int32_t bestSector[SECTORS];
    int32_t refSector[SECTORS];
    SectorState lastLapSector[SECTORS];
    uint32_t sectorRev = 0;

    MapData map;
    uint32_t mapRev = 0;
    int offTrackChecks = 0;
    uint32_t trackCheckCounter = 0;

    LapRecord sessionList[SESSION_LAPS];
    int sessionHead = 0, sessionStored = 0, sessionLaps = 0;
    int32_t sessionBest = -1;
    int64_t sessionTotalMs = 0;
    float sessionTopKmh = 0;
    uint32_t sessionRev = 0;

    // Motion: heading, yaw rate and accelerations.
    bool hasHeading = false, hasAnchor = false, hasSpeedRef = false;
    float heading = 0, yawRate = 0, anchorX = 0, anchorZ = 0, speedRef = 0;
    uint32_t headingMs = 0, speedRefMs = 0;
    float latAccel = 0, longAccel = 0;
    float peakLat = 0, peakBrake = 0, peakAccel = 0;
    float trailLat[G_TRAIL], trailLong[G_TRAIL];
    int trailHead = 0, trailCount = 0;
    uint32_t trailMs = 0;
    uint8_t traceThr[TRACE_SAMPLES], traceBrk[TRACE_SAMPLES];
    int traceHead = 0, traceSamples = 0;
    uint32_t traceMs = 0, traceRev = 0;

    LaunchState launch;
    LaunchRun lastRun, bestRun;
    uint32_t launchRev = 0;
    uint32_t rollingStartMs = 0;
    bool rollingActive = false;

    bool fuelArmed = true;
    float fuelLowest = NAN;

    // Braking points.
    struct BrakeTracker
    {
        bool active = false;      // pedal down, zone being followed
        bool confirmed = false;
        bool released = false;    // pedal below BRAKE_OFF, waiting to end
        uint32_t startMs = 0;
        uint32_t releasedMs = 0;
        uint32_t lastEndMs = 0;
        bool hasEnded = false;
        BrakeZone zone;
        float startSpeed = 0;
    };
    BrakeTracker braking;
    int curZoneCount = 0, refZoneCount = 0;
    int nextRef = 0;              // next reference zone of this lap
    bool nextRefValid = false;
    int resultIndex = -1;         // reference zone the last braking was matched with
    float resultDelta = NAN;      // metres: + braked earlier than the reference
    float resultEntryKmh = NAN, resultReferenceKmh = NAN;
    uint32_t brakeRev = 0, refZoneRev = 0;

    // Persistence.
    uint32_t trackKeyValue = 0;
    bool saveTrackPending = false, saveReferencePending = false;

    Event events[EVENT_QUEUE];
    int eventHead = 0, eventCount = 0;

    // ---- helpers ----
    static bool valid(int sector) { return sector >= 0 && sector < SECTORS; }
    static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
    int traceIndex(int age) const { return (traceHead - 1 - age + 2 * TRACE_SAMPLES) % TRACE_SAMPLES; }

    // Lap time of the sample. Right after the line the game may still send
    // the time of the lap just finished for a packet or two: ignore it.
    int32_t lapTimeOf(const Sample &s) const
    {
        if (s.lapTimeMs < 0) return -1;
        if (lap.observed && s.lapTimeMs > static_cast<int32_t>(s.timeMs - lap.startClockMs) + 1500)
            return -1;
        return s.lapTimeMs;
    }

    void push(EventType type, int32_t lapMs, int32_t gainMs)
    {
        if (eventCount == EVENT_QUEUE) { eventHead = (eventHead + 1) % EVENT_QUEUE; eventCount--; }
        Event &e = events[(eventHead + eventCount) % EVENT_QUEUE];
        e.type = type; e.lapMs = lapMs; e.gainMs = gainMs;
        eventCount++;
    }

    void resetMotion()
    {
        hasHeading = hasAnchor = hasSpeedRef = false;
        yawRate = latAccel = longAccel = 0;
        trailHead = trailCount = 0;
        traceHead = traceSamples = 0;
        trailMs = traceMs = 0;
        motionGap = true;
        for (int i = 0; i < G_TRAIL; ++i) trailLat[i] = trailLong[i] = 0;
        memset(traceThr, 0, sizeof(traceThr));
        memset(traceBrk, 0, sizeof(traceBrk));
        traceRev++;
    }

    // A rewind, restart or teleport breaks the lap: nothing measured since the
    // last line crossing can be trusted any more.
    void breakLap()
    {
        lap.continuous = false;
        lap.observed = false;
        braking.active = false;
        nextRefValid = false; // after a rewind the next braking point is searched again
        if (map.state == MapState::Recording) { map.state = MapState::Waiting; map.rawCount = 0; mapRev++; }
        // Without the fraction of the lap, the distance from the line is lost.
        if (!lapFractionMode) lap.progressKnown = false;
    }
    bool lapFractionMode = false;

    void checkContinuity(const Sample &s)
    {
        if (!hasPrevious) return;
        bool broken = false;
        if (s.hasPosition && previous.hasPosition)
        {
            const float dx = s.x - previous.x, dz = s.z - previous.z;
            if (dx * dx + dz * dz > JUMP_M * JUMP_M) broken = true;
        }
        // Lap time going backwards without a new lap: rewind or restart.
        const int32_t lapTime = lapTimeOf(s);
        if (s.lapCount == previous.lapCount && lapTime >= 0 && lap.lastTime >= 0 &&
            lapTime + 500 < lap.lastTime)
            broken = true;
        if (broken) breakLap();
    }

    void updateLapCounter(const Sample &s, int32_t dtMs)
    {
        lapFractionMode = isfinite(s.lapFraction);
        if (lapFractionMode) lapCells = DELTA_CELLS;
        if (!hasLapCounter)
        {
            hasLapCounter = true;
            lapCounter = s.lapCount;
            beginLap(s, false);
            return;
        }
        if (s.lapCount == lapCounter) return;
        if (s.lapCount == lapCounter + 1)
        {
            completeLap(s, dtMs);
            lapCounter = s.lapCount;
            beginLap(s, true);
            if (s.totalLaps >= 2 && s.lapCount == s.totalLaps) push(EventType::FinalLap, -1, -1);
            return;
        }
        // Restart (counter going back) or laps skipped: wait for the next line.
        lapCounter = s.lapCount;
        beginLap(s, false);
    }

    void beginLap(const Sample &s, bool fromLine)
    {
        startBrakeLap(fromLine);
        lap = LapState();
        lap.observed = fromLine;
        lap.startClockMs = s.timeMs;
        // A race starts from the grid, behind the line: that lap is timed but
        // its distances are shifted, so it gives no map, reference or sectors.
        lap.standingStart = fromLine && s.speed < 5.0f && !isfinite(s.lapFraction);
        lap.continuous = true;
        lap.lastTime = lapTimeOf(s);
        lap.progressKnown = fromLine || isfinite(s.lapFraction);
        if (lap.progressKnown)
        {
            lap.progress = progressCells(s);
            lap.lastProgress = lap.progress;
            if (fromLine)
            {
                // The cells up to here belong to the line.
                const uint32_t t = lap.lastTime >= 0 ? static_cast<uint32_t>(lap.lastTime) : 0;
                const int cells = static_cast<int>(floorf(lap.progress));
                for (int c = 0; c <= cells && c < DELTA_CELLS; ++c) buffers->curTime[c] = t;
                lap.maxCell = cells < DELTA_CELLS ? cells : DELTA_CELLS - 1;
            }
            lap.sector = sectorOf(lap.progress);
        }
        for (int i = 0; i < SECTORS; ++i)
        {
            lap.sectorState[i] = SectorState::None;
            lap.sectorMs[i] = -1;
            lap.sectorStart[i] = -1;
        }
        lap.sectorStart[0] = 0;
        if (sectorsAvailable() && lap.progressKnown && !lap.standingStart)
            lap.sectorState[lap.sector] = SectorState::Running;
        sectorRev++;

        if (fromLine && !lap.standingStart && map.state == MapState::Waiting && s.hasPosition && !s.inPit)
        {
            map.state = MapState::Recording;
            map.rawCount = 0;
            map.spacing = MAP_FIRST_SPACING_M;
            map.lastRawDistance = 0;
            appendRaw(s.x, s.z);
            mapRev++;
        }
    }

    void completeLap(const Sample &s, int32_t dtMs)
    {
        if (!lap.observed) return;
        // Lap time: the official one when it is already updated, otherwise
        // measured, corrected later if the official value arrives.
        int32_t measured = -1;
        if (lap.lastTime >= 0)
        {
            measured = lap.lastTime;
            if (s.lapTimeMs >= 0 && dtMs > 0 && dtMs < 1000 && s.lapTimeMs <= dtMs)
                measured += dtMs - s.lapTimeMs;
        }
        const bool officialNew = s.lastLapMs > 0 && s.lastLapMs != officialLastSeen;
        const int32_t lapMs = officialNew ? s.lastLapMs : measured;
        if (lapMs <= 0) return;
        pendingOfficialSamples = officialNew ? 0 : OFFICIAL_WAIT_SAMPLES;
        pendingMeasuredMs = officialNew ? -1 : lapMs;

        // Last sector ends at the line.
        if (sectorsAvailable() && lap.progressKnown && !lap.standingStart)
        {
            const int last = SECTORS - 1;
            if (lap.sectorStart[last] >= 0 && lap.sectorMs[last] < 0)
                finishSector(last, lapMs - lap.sectorStart[last]);
        }

        // Fraction mode: the line is the last cell; fill the cells after the
        // last sample up to it.
        if (lapFractionMode && lap.progressKnown && lap.maxCell >= 0 && lap.lastTime >= 0)
        {
            const float p0 = lap.lastProgress, end = static_cast<float>(DELTA_CELLS - 1);
            for (int c = lap.maxCell + 1; c < DELTA_CELLS; ++c)
            {
                const float k = end > p0 ? (c - p0) / (end - p0) : 1.0f;
                buffers->curTime[c] = static_cast<uint32_t>(lap.lastTime + (lapMs - lap.lastTime) * clampf(k, 0, 1));
            }
            lap.maxCell = DELTA_CELLS - 1;
        }

        // Lap length in cells, learned from the first complete lap (GT7).
        const int cellsDriven = lap.maxCell + 1;
        const bool fullCoverage = lapFractionMode
            ? lap.continuous && lap.maxCell >= DELTA_CELLS * 9 / 10
            : lap.continuous && !lap.standingStart && cellsDriven >= 8;
        if (!lapFractionMode && fullCoverage && lapCells == 0) lapCells = cellsDriven;
        if (lapFractionMode) lapCells = DELTA_CELLS;

        // First GT7 lap: the sector boundaries were unknown while driving it,
        // the times at each boundary are in the cells.
        if (fullCoverage && lapCells > 0 && lap.sectorMs[0] < 0 && lap.sectorMs[SECTORS - 1] < 0)
        {
            int32_t start = 0;
            for (int i = 0; i < SECTORS; ++i)
            {
                const int32_t end = i == SECTORS - 1 ? lapMs
                    : cellTime(static_cast<float>(lapCells) * (i + 1) / SECTORS, cellsDriven);
                if (end < 0) break;
                finishSector(i, end - start);
                start = end;
            }
        }

        LapRecord record;
        record.lapMs = lapMs;
        for (int i = 0; i < SECTORS; ++i) record.sectorMs[i] = lap.sectorMs[i];
        record.topSpeedKmh = lap.topKmh;
        storeLap(record);

        if (sessionBest < 0 || lapMs < sessionBest)
        {
            push(EventType::BestLap, lapMs, sessionBest > 0 ? sessionBest - lapMs : -1);
            sessionBest = lapMs;
        }
        if (fullCoverage && (refLapMs < 0 || lapMs < refLapMs))
        {
            refLapMs = lapMs;
            refCells = lapFractionMode ? DELTA_CELLS : cellsDriven;
            memcpy(buffers->refTime, buffers->curTime, sizeof(uint32_t) * refCells);
            if (!lapFractionMode) lapCells = refCells;
            for (int i = 0; i < SECTORS; ++i) refSector[i] = lap.sectorMs[i];
            promoteBrakeZones();
            saveReferencePending = true;
        }
        for (int i = 0; i < SECTORS; ++i) lastLapSector[i] = lap.sectorState[i];
        sectorRev++;

        if (map.state == MapState::Recording && lap.continuous) finishMap();
        else if (map.state == MapState::Recording) { map.state = MapState::Waiting; mapRev++; }
    }

    // Lap time recorded at a point of the current lap, -1 outside it.
    int32_t cellTime(float p, int cells) const
    {
        const int c = static_cast<int>(floorf(p));
        if (c < 0 || c + 1 >= cells) return -1;
        const float f = p - c;
        return static_cast<int32_t>(buffers->curTime[c] + (static_cast<float>(buffers->curTime[c + 1]) - buffers->curTime[c]) * f);
    }

    void storeLap(const LapRecord &record)
    {
        sessionList[sessionHead] = record;
        sessionHead = (sessionHead + 1) % SESSION_LAPS;
        if (sessionStored < SESSION_LAPS) sessionStored++;
        sessionLaps++;
        sessionTotalMs += record.lapMs;
        sessionRev++;
    }

    // GT7 may publish the official lap time a few packets after the line.
    void updateOfficialLapTime(const Sample &s)
    {
        if (pendingOfficialSamples > 0)
        {
            pendingOfficialSamples--;
            if (s.lastLapMs > 0 && s.lastLapMs != officialLastSeen && sessionStored > 0)
            {
                LapRecord &last = sessionList[(sessionHead - 1 + SESSION_LAPS) % SESSION_LAPS];
                const int32_t diff = s.lastLapMs - last.lapMs;
                if (diff > -2000 && diff < 2000 && last.lapMs == pendingMeasuredMs)
                {
                    sessionTotalMs += diff;
                    if (sessionBest == last.lapMs) sessionBest = s.lastLapMs;
                    if (refLapMs == last.lapMs) refLapMs = s.lastLapMs;
                    last.lapMs = s.lastLapMs;
                    sessionRev++;
                }
                pendingOfficialSamples = 0;
            }
        }
        officialLastSeen = s.lastLapMs;
    }

    float progressCells(const Sample &s) const
    {
        if (isfinite(s.lapFraction))
        {
            const float f = s.lapFraction;
            // The fraction wraps at the start of the spline, which can be a few
            // metres away from the timing line.
            if (lap.maxCell < DELTA_CELLS / 2 && f > 0.9f) return 0.0f;
            if (lap.maxCell > DELTA_CELLS / 2 && f < 0.1f) return static_cast<float>(DELTA_CELLS - 1);
            return clampf(f, 0.0f, 1.0f) * (DELTA_CELLS - 1);
        }
        return lap.distance / DISTANCE_CELL_M;
    }

    int sectorOf(float progress) const
    {
        if (lapCells <= 0) return 0;
        const int sector = static_cast<int>(progress * SECTORS / lapCells);
        return sector < 0 ? 0 : sector >= SECTORS ? SECTORS - 1 : sector;
    }

    void finishSector(int i, int32_t ms)
    {
        if (ms <= 0) return;
        lap.sectorMs[i] = ms;
        SectorState state = SectorState::Yellow;
        if (bestSector[i] < 0 || ms < bestSector[i]) state = SectorState::Purple;
        else if (refSector[i] > 0 && ms < refSector[i]) state = SectorState::Green;
        if (bestSector[i] < 0 || ms < bestSector[i]) bestSector[i] = ms;
        lap.sectorState[i] = state;
        sectorRev++;
    }

    void updateProgress(const Sample &s)
    {
        // Distance driven since the line, from the positions when available.
        if (hasPrevious && s.hasPosition && previous.hasPosition)
        {
            const float dx = s.x - previous.x, dy = s.y - previous.y, dz = s.z - previous.z;
            const float step = sqrtf(dx * dx + dy * dy + dz * dz);
            if (step < JUMP_M) lap.distance += step;
        }
        else if (hasPrevious)
        {
            const int32_t dt = static_cast<int32_t>(s.timeMs - previous.timeMs);
            if (dt > 0 && dt < 1000) lap.distance += s.speed * dt / 1000.0f;
        }

        liveDeltaValid = false;
        const int32_t t = lapTimeOf(s);
        if (!lap.progressKnown || t < 0) { if (t >= 0) lap.lastTime = t; return; }

        const float p = progressCells(s);
        const float p0 = lap.lastProgress;
        const int32_t t0 = lap.lastTime;

        if (t0 >= 0 && p > p0)
        {
            // Lap time at every cell crossed since the last sample.
            int first = static_cast<int>(floorf(p0)) + 1;
            if (first <= lap.maxCell) first = lap.maxCell + 1;
            const int lastCell = static_cast<int>(floorf(p));
            for (int c = first; c <= lastCell && c < DELTA_CELLS; ++c)
            {
                const float k = (c - p0) / (p - p0);
                buffers->curTime[c] = static_cast<uint32_t>(t0 + (t - t0) * k);
                // Joining mid-lap (fraction mode) leaves the cells before unknown.
                if (lap.maxCell < 0 && c > 0 && !lap.observed) lap.continuous = false;
                lap.maxCell = c;
            }
            // Sector boundaries crossed.
            if (sectorsAvailable() && !lap.standingStart)
            {
                for (int b = 1; b < SECTORS; ++b)
                {
                    const float boundary = static_cast<float>(lapCells) * b / SECTORS;
                    if (p0 < boundary && p >= boundary)
                    {
                        const int32_t cross = static_cast<int32_t>(t0 + (t - t0) * ((boundary - p0) / (p - p0)));
                        if (lap.sectorStart[b - 1] >= 0) finishSector(b - 1, cross - lap.sectorStart[b - 1]);
                        lap.sectorStart[b] = cross;
                        lap.sector = b;
                        lap.sectorState[b] = SectorState::Running;
                        sectorRev++;
                    }
                }
            }
        }
        if (p > lap.lastProgress) lap.lastProgress = p;
        lap.progress = p;
        lap.lastTime = t;

        // Live delta against the reference lap at the same point of the lap.
        if (refLapMs > 0 && refCells > 1 && !lap.standingStart && (lap.observed || lapFractionMode))
        {
            const int c = static_cast<int>(floorf(p));
            if (c >= 0 && c + 1 < refCells)
            {
                const float f = p - c;
                const float ref = buffers->refTime[c] + (static_cast<float>(buffers->refTime[c + 1]) - buffers->refTime[c]) * f;
                liveDelta = static_cast<int32_t>(lroundf(t - ref));
                liveDeltaValid = true;
            }
        }
    }

    void appendRaw(float x, float z)
    {
        if (map.rawCount == MAP_RAW_POINTS)
        {
            // Full: keep every other point and double the spacing.
            for (int i = 0; i < MAP_RAW_POINTS / 2; ++i)
            {
                buffers->rawX[i] = buffers->rawX[i * 2];
                buffers->rawZ[i] = buffers->rawZ[i * 2];
            }
            map.rawCount = MAP_RAW_POINTS / 2;
            map.spacing *= 2;
        }
        if (map.rawCount == 0) { map.minX = map.maxX = x; map.minZ = map.maxZ = z; }
        buffers->rawX[map.rawCount] = x;
        buffers->rawZ[map.rawCount] = z;
        map.rawCount++;
        if (x < map.minX) map.minX = x;
        if (x > map.maxX) map.maxX = x;
        if (z < map.minZ) map.minZ = z;
        if (z > map.maxZ) map.maxZ = z;
    }

    void updateMap(const Sample &s)
    {
        if (map.state != MapState::Recording || !s.hasPosition) return;
        if (s.inPit) { map.state = MapState::Waiting; map.rawCount = 0; mapRev++; return; }
        if (lap.distance - map.lastRawDistance >= map.spacing)
        {
            appendRaw(s.x, s.z);
            map.lastRawDistance = lap.distance;
            mapRev++;
        }
    }

    // Resamples the recorded lap to MAP_POINTS points evenly spaced along the
    // closed track.
    void finishMap()
    {
        const int n = map.rawCount;
        float total = 0;
        for (int i = 0; i < n; ++i)
        {
            const int j = (i + 1) % n;
            total += hypotf(buffers->rawX[j] - buffers->rawX[i], buffers->rawZ[j] - buffers->rawZ[i]);
        }
        if (n < 24 || total < 300.0f) { map.state = MapState::Waiting; map.rawCount = 0; mapRev++; return; }

        const float step = total / MAP_POINTS;
        int segment = 0;
        float segmentStart = 0;
        float segmentLength = hypotf(buffers->rawX[1 % n] - buffers->rawX[0], buffers->rawZ[1 % n] - buffers->rawZ[0]);
        for (int k = 0; k < MAP_POINTS; ++k)
        {
            const float target = k * step;
            while (segmentStart + segmentLength < target && segment < n - 1)
            {
                segmentStart += segmentLength;
                segment++;
                const int next = (segment + 1) % n;
                segmentLength = hypotf(buffers->rawX[next] - buffers->rawX[segment], buffers->rawZ[next] - buffers->rawZ[segment]);
            }
            const int next = (segment + 1) % n;
            const float f = segmentLength > 0 ? clampf((target - segmentStart) / segmentLength, 0, 1) : 0;
            buffers->mapX[k] = buffers->rawX[segment] + (buffers->rawX[next] - buffers->rawX[segment]) * f;
            buffers->mapZ[k] = buffers->rawZ[segment] + (buffers->rawZ[next] - buffers->rawZ[segment]) * f;
        }
        map.minX = map.maxX = buffers->mapX[0];
        map.minZ = map.maxZ = buffers->mapZ[0];
        for (int k = 1; k < MAP_POINTS; ++k)
        {
            if (buffers->mapX[k] < map.minX) map.minX = buffers->mapX[k];
            if (buffers->mapX[k] > map.maxX) map.maxX = buffers->mapX[k];
            if (buffers->mapZ[k] < map.minZ) map.minZ = buffers->mapZ[k];
            if (buffers->mapZ[k] > map.maxZ) map.maxZ = buffers->mapZ[k];
        }
        map.state = MapState::Complete;
        mapRev++;
        trackKeyValue = computeTrackKey(total);
        saveTrackPending = true;
    }

    // Far from the recorded map for a while: another circuit, start over.
    void checkTrackChange(const Sample &s)
    {
        if (map.state != MapState::Complete || !s.hasPosition || s.speed < 5.0f) return;
        if (++trackCheckCounter % 15 != 0) return;
        float best = 1e12f;
        for (int k = 0; k < MAP_POINTS; ++k)
        {
            const float dx = buffers->mapX[k] - s.x, dz = buffers->mapZ[k] - s.z;
            const float d = dx * dx + dz * dz;
            if (d < best) best = d;
        }
        offTrackChecks = best > 200.0f * 200.0f ? offTrackChecks + 1 : 0;
        if (offTrackChecks > 40)
        {
            const bool fractionMode = lapFractionMode;
            resetTrack();
            lapFractionMode = fractionMode;
        }
    }

    void updateMotion(const Sample &s, int32_t dtMs)
    {
        if (motionGap || dtMs < 0 || dtMs > 500)
        {
            hasHeading = hasAnchor = hasSpeedRef = false;
            yawRate = latAccel = longAccel = 0;
        }

        // Heading of the car on the map, from the velocity (GT7) or from the
        // positions (Assetto Corsa).
        bool newHeading = false;
        float h = 0;
        if (s.hasVelocity && s.speed > 3.0f)
        {
            h = atan2f(s.vz, s.vx);
            newHeading = true;
        }
        else if (s.hasPosition)
        {
            if (!hasAnchor) { anchorX = s.x; anchorZ = s.z; hasAnchor = true; }
            const float dx = s.x - anchorX, dz = s.z - anchorZ;
            if (dx * dx + dz * dz >= 1.5f * 1.5f)
            {
                h = atan2f(dz, dx);
                anchorX = s.x; anchorZ = s.z;
                newHeading = true;
            }
        }
        if (newHeading)
        {
            if (hasHeading)
            {
                const float dt = static_cast<int32_t>(s.timeMs - headingMs) / 1000.0f;
                if (dt > 0.0f && dt < 0.5f)
                {
                    float d = h - heading;
                    while (d > 3.14159265f) d -= 6.2831853f;
                    while (d < -3.14159265f) d += 6.2831853f;
                    yawRate += 0.35f * (d / dt - yawRate);
                }
            }
            heading = h;
            headingMs = s.timeMs;
            hasHeading = true;
        }
        // In screen coordinates (z downwards) a growing heading turns right.
        latAccel = s.speed > 2.0f ? clampf(s.speed * yawRate, -60.0f, 60.0f) : 0.0f;

        if (!hasSpeedRef) { speedRef = s.speed; speedRefMs = s.timeMs; hasSpeedRef = true; }
        const int32_t speedDt = static_cast<int32_t>(s.timeMs - speedRefMs);
        if (speedDt >= 50)
        {
            const float a = (s.speed - speedRef) * 1000.0f / speedDt;
            longAccel += 0.35f * (clampf(a, -80.0f, 80.0f) - longAccel);
            speedRef = s.speed;
            speedRefMs = s.timeMs;
        }

        if (s.speed > 5.0f)
        {
            const float lat = fabsf(latAccel);
            if (lat > peakLat) peakLat = lat;
            if (-longAccel > peakBrake) peakBrake = -longAccel;
            if (longAccel > peakAccel) peakAccel = longAccel;
        }

        if (static_cast<int32_t>(s.timeMs - trailMs) >= static_cast<int32_t>(G_TRAIL_STEP_MS) || trailCount == 0)
        {
            trailMs = s.timeMs;
            trailLat[trailHead] = lateralG();
            trailLong[trailHead] = longitudinalG();
            trailHead = (trailHead + 1) % G_TRAIL;
            if (trailCount < G_TRAIL) trailCount++;
        }
        if (static_cast<int32_t>(s.timeMs - traceMs) >= static_cast<int32_t>(TRACE_STEP_MS) || traceSamples == 0)
        {
            traceMs = s.timeMs;
            traceThr[traceHead] = static_cast<uint8_t>(lroundf(clampf(s.throttle, 0, 1) * 100));
            traceBrk[traceHead] = static_cast<uint8_t>(lroundf(clampf(s.brake, 0, 1) * 100));
            traceHead = (traceHead + 1) % TRACE_SAMPLES;
            if (traceSamples < TRACE_SAMPLES) traceSamples++;
            traceRev++;
        }
    }

    // Time when the speed crossed `level` between the previous sample and this one.
    uint32_t crossingMs(const Sample &s, float level) const
    {
        const float v0 = previous.speed, v1 = s.speed;
        if (v1 == v0) return s.timeMs;
        const float k = clampf((level - v0) / (v1 - v0), 0, 1);
        return previous.timeMs + static_cast<uint32_t>((s.timeMs - previous.timeMs) * k);
    }

    static void keepBest(float &best, float value, bool lower)
    {
        if (!isfinite(value)) return;
        if (!isfinite(best) || (lower ? value < best : value > best)) best = value;
    }

    void finishRun(uint32_t nowMs)
    {
        launch.phase = LaunchPhase::Finished;
        launch.durationS = (nowMs - launch.startMs) / 1000.0f;
        lastRun = launch.run;
        keepBest(bestRun.t100, lastRun.t100, true);
        keepBest(bestRun.t200, lastRun.t200, true);
        keepBest(bestRun.t100to200, lastRun.t100to200, true);
        keepBest(bestRun.t400, lastRun.t400, true);
        if (isfinite(lastRun.t400) && (!isfinite(bestRun.t400) || lastRun.t400 <= bestRun.t400))
            bestRun.v400Kmh = lastRun.v400Kmh;
        keepBest(bestRun.vmaxKmh, lastRun.vmaxKmh, false);
        if (isfinite(lastRun.reaction) && lastRun.reaction >= 0) keepBest(bestRun.reaction, lastRun.reaction, true);
        launchRev++;
    }

    void updateLaunch(const Sample &s, int32_t dtMs)
    {
        const bool stopped = s.speed < STOPPED_MS;
        if (stopped && !launch.stopped) launch.stoppedSinceMs = s.timeMs;
        launch.stopped = stopped;

        switch (launch.phase)
        {
        case LaunchPhase::Rolling:
        case LaunchPhase::Finished:
            if (stopped && s.timeMs - launch.stoppedSinceMs >= STAGE_MS)
            {
                launch.phase = LaunchPhase::Staged;
                launch.stagedMs = s.timeMs;
                launchRev++;
            }
            break;
        case LaunchPhase::Staged:
            if (!stopped && hasPrevious)
            {
                // Start time extrapolated back to standstill with the current
                // acceleration, as a GPS performance meter does.
                uint32_t start = crossingMs(s, STOPPED_MS);
                const int32_t stepMs = static_cast<int32_t>(s.timeMs - previous.timeMs);
                if (stepMs > 0 && stepMs < 1000 && s.speed > previous.speed)
                {
                    const float accel = (s.speed - previous.speed) * 1000.0f / stepMs;
                    const int32_t sinceStart = static_cast<int32_t>(lroundf(s.speed / accel * 1000.0f));
                    if (sinceStart <= 1000) start = s.timeMs - sinceStart;
                }
                launch.phase = LaunchPhase::Running;
                launch.startMs = start;
                launch.distance = 0.5f * s.speed * static_cast<int32_t>(s.timeMs - start) / 1000.0f;
                launch.run = LaunchRun();
                // Reaction time only when the ambers were already lit: a short
                // stop in traffic is not a false start.
                const int32_t sinceStage = static_cast<int32_t>(start - launch.stagedMs);
                launch.falseStart = false;
                launch.treeUsed = sinceStage >= static_cast<int32_t>(TREE_AMBER_MS);
                if (launch.treeUsed)
                {
                    const float reaction = (sinceStage - static_cast<int32_t>(TREE_GREEN_MS)) / 1000.0f;
                    launch.falseStart = reaction < 0;
                    if (reaction < 2.0f) launch.run.reaction = reaction;
                }
                launch.run.vmaxKmh = s.speed * 3.6f;
                launchRev++;
            }
            break;
        case LaunchPhase::Running:
        {
            const float dt = dtMs > 0 && dtMs < 1000 ? dtMs / 1000.0f : 0;
            const float before = launch.distance;
            launch.distance += (s.speed + previous.speed) * 0.5f * dt;
            LaunchRun &run = launch.run;
            const float v100 = 100.0f / 3.6f, v200 = 200.0f / 3.6f;
            if (!isfinite(run.t100) && previous.speed < v100 && s.speed >= v100)
            {
                run.t100 = (crossingMs(s, v100) - launch.startMs) / 1000.0f;
                launchRev++;
            }
            if (!isfinite(run.t200) && previous.speed < v200 && s.speed >= v200)
            {
                run.t200 = (crossingMs(s, v200) - launch.startMs) / 1000.0f;
                if (isfinite(run.t100)) run.t100to200 = run.t200 - run.t100;
                launchRev++;
            }
            if (!isfinite(run.t400) && before < 400.0f && launch.distance >= 400.0f)
            {
                const float k = launch.distance > before ? (400.0f - before) / (launch.distance - before) : 1.0f;
                run.t400 = (previous.timeMs + (s.timeMs - previous.timeMs) * k - launch.startMs) / 1000.0f;
                run.v400Kmh = (previous.speed + (s.speed - previous.speed) * k) * 3.6f;
                launchRev++;
            }
            const float kmh = s.speed * 3.6f;
            if (kmh > run.vmaxKmh) run.vmaxKmh = kmh;
            // The run ends on the brakes, lifting off or after a minute.
            if (s.brake > 0.2f || kmh < run.vmaxKmh - 15.0f || stopped ||
                s.timeMs - launch.startMs > 60000)
                finishRun(s.timeMs);
            break;
        }
        }

        // Rolling 100-200 km/h, also outside standing starts.
        const float v100 = 100.0f / 3.6f, v95 = 95.0f / 3.6f, v200 = 200.0f / 3.6f;
        if (hasPrevious && dtMs > 0 && dtMs < 1000)
        {
            if (!rollingActive && previous.speed < v100 && s.speed >= v100)
            {
                rollingActive = true;
                rollingStartMs = crossingMs(s, v100);
            }
            else if (rollingActive && s.speed < v95) rollingActive = false;
            else if (rollingActive && previous.speed < v200 && s.speed >= v200)
            {
                rollingActive = false;
                const float t = (crossingMs(s, v200) - rollingStartMs) / 1000.0f;
                if (launch.phase != LaunchPhase::Running) lastRun.t100to200 = t;
                keepBest(bestRun.t100to200, t, true);
                launchRev++;
            }
        }
    }

    // ---- Braking points --------------------------------------------------------------
    void clearBrakeZones(bool includeReference)
    {
        braking = BrakeTracker();
        curZoneCount = 0;
        nextRef = 0;
        nextRefValid = false;
        resultIndex = -1;
        resultDelta = resultEntryKmh = resultReferenceKmh = NAN;
        if (buffers)
            for (int i = 0; i < MAX_BRAKE_ZONES; ++i) buffers->curDelta[i] = buffers->lastDelta[i] = NAN;
        if (includeReference) { refZoneCount = 0; refZoneRev++; }
        brakeRev++;
    }

    // A lap begins: the results of the one just finished become "previous".
    void startBrakeLap(bool fromLine)
    {
        if (!buffers) return;
        if (fromLine)
        {
            for (int i = 0; i < MAX_BRAKE_ZONES; ++i)
            {
                buffers->lastDelta[i] = buffers->curDelta[i];
                buffers->curDelta[i] = NAN;
            }
            nextRef = 0;
            nextRefValid = true;
        }
        else
        {
            for (int i = 0; i < MAX_BRAKE_ZONES; ++i) buffers->curDelta[i] = NAN;
            nextRefValid = false; // joined in the middle of a lap: found again from the position
        }
        curZoneCount = 0;
        brakeRev++;
    }

    // This lap is the new reference: its braking points are the ones to beat.
    void promoteBrakeZones()
    {
        if (!buffers || curZoneCount == 0) return;
        for (int i = 0; i < curZoneCount; ++i) buffers->refZones[i] = buffers->curZones[i];
        refZoneCount = curZoneCount;
        // The results of this lap were against the old reference.
        for (int i = 0; i < MAX_BRAKE_ZONES; ++i) buffers->curDelta[i] = NAN;
        brakeRev++;
        refZoneRev++;
    }

    // Reference braking point of the same corner: close ahead or behind, same
    // direction, not matched yet in this lap. Returns the index or -1; delta
    // is how many metres earlier (+) the pedal went down.
    int matchReference(const BrakeZone &z, float &delta) const
    {
        int best = -1;
        float bestDistance = 1e9f;
        for (int j = 0; j < refZoneCount; ++j)
        {
            if (isfinite(buffers->curDelta[j])) continue;
            const BrakeZone &ref = buffers->refZones[j];
            if (ref.hx * z.hx + ref.hz * z.hz < 0.7f) continue;
            const float dx = ref.x - z.x, dz = ref.z - z.z;
            const float along = dx * z.hx + dz * z.hz;
            const float side = -dx * z.hz + dz * z.hx;
            if (fabsf(along) > BRAKE_MATCH_ALONG_M || fabsf(side) > BRAKE_MATCH_SIDE_M) continue;
            const float distance = hypotf(along, side);
            if (distance < bestDistance) { bestDistance = distance; best = j; delta = along; }
        }
        return best;
    }

    // Keeps track of the next reference braking point of this lap.
    void updateNextReference(const Sample &s)
    {
        if (refZoneCount == 0) return;
        const float hx = cosf(heading), hz = sinf(heading);
        if (!nextRefValid)
        {
            int best = -1;
            float bestAhead = 1e9f;
            for (int j = 0; j < refZoneCount; ++j)
            {
                const BrakeZone &ref = buffers->refZones[j];
                const float dx = ref.x - s.x, dz = ref.z - s.z;
                const float ahead = dx * hx + dz * hz, side = -dx * hz + dz * hx;
                if (ahead > 0.0f && ahead < 800.0f && fabsf(side) < 60.0f && ahead < bestAhead)
                {
                    bestAhead = ahead;
                    best = j;
                }
            }
            if (best >= 0) { nextRef = best; nextRefValid = true; brakeRev++; }
            return;
        }
        // Past the point (and close to it): on to the next one.
        while (nextRef < refZoneCount)
        {
            const BrakeZone &ref = buffers->refZones[nextRef];
            const float dx = ref.x - s.x, dz = ref.z - s.z;
            if (dx * hx + dz * hz < -20.0f && hypotf(dx, dz) < 120.0f) { nextRef++; brakeRev++; }
            else break;
        }
    }

    void confirmBrakeZone()
    {
        braking.confirmed = true;
        const BrakeZone &z = braking.zone;
        if (curZoneCount < MAX_BRAKE_ZONES) buffers->curZones[curZoneCount++] = z;
        float delta = NAN;
        const int match = matchReference(z, delta);
        resultIndex = match;
        resultEntryKmh = z.entryKmh;
        if (match >= 0)
        {
            buffers->curDelta[match] = delta;
            nextRef = match + 1;
            nextRefValid = true;
            resultDelta = delta;
            resultReferenceKmh = buffers->refZones[match].entryKmh;
        }
        else
        {
            resultDelta = NAN;
            resultReferenceKmh = NAN;
        }
        brakeRev++;
    }

    void updateBrakeZones(const Sample &s)
    {
        if (!buffers || !s.hasPosition || !hasHeading) { braking.active = false; return; }
        updateNextReference(s);

        if (!braking.active)
        {
            if (s.brake < BRAKE_ON || s.speed < BRAKE_MIN_SPEED) return;
            if (braking.hasEnded && static_cast<int32_t>(s.timeMs - braking.lastEndMs) < static_cast<int32_t>(BRAKE_GAP_MS))
                return;
            braking.active = true;
            braking.confirmed = false;
            braking.released = false;
            braking.startMs = s.timeMs;
            braking.startSpeed = s.speed;
            braking.zone.x = s.x; braking.zone.z = s.z;
            braking.zone.hx = cosf(heading); braking.zone.hz = sinf(heading);
            braking.zone.entryKmh = s.speed * 3.6f;
            braking.zone.peak = s.brake;
            return;
        }

        const int32_t sinceStart = static_cast<int32_t>(s.timeMs - braking.startMs);
        if (sinceStart < 0) { braking.active = false; return; } // the clock went back
        if (s.brake > braking.zone.peak) braking.zone.peak = s.brake;
        if (s.brake < BRAKE_OFF)
        {
            if (!braking.released) { braking.released = true; braking.releasedMs = s.timeMs; }
            else if (static_cast<int32_t>(s.timeMs - braking.releasedMs) >= static_cast<int32_t>(BRAKE_RELEASE_MS))
            {
                braking.active = false;
                braking.hasEnded = true;
                braking.lastEndMs = s.timeMs;
            }
            return;
        }
        braking.released = false;
        if (!braking.confirmed && sinceStart >= static_cast<int32_t>(BRAKE_CONFIRM_MS))
        {
            // A real braking slows the car down; a brush of the pedal does not.
            if (braking.startSpeed - s.speed >= BRAKE_MIN_DROP) confirmBrakeZone();
            else { braking.active = false; braking.hasEnded = true; braking.lastEndMs = s.timeMs; }
        }
    }

    uint32_t computeTrackKey(float totalLength) const
    {
        // The start line (to 10 m) and the length (to 50 m) name the circuit.
        const int32_t parts[3] = {static_cast<int32_t>(lroundf(0.1f * buffers->mapX[0])),
                                  static_cast<int32_t>(lroundf(0.1f * buffers->mapZ[0])),
                                  static_cast<int32_t>(lroundf(totalLength / 50.0f))};
        uint32_t hash = 2166136261u;
        for (int i = 0; i < 3; ++i)
            for (int b = 0; b < 4; ++b)
            {
                hash ^= (static_cast<uint32_t>(parts[i]) >> (8 * b)) & 0xFF;
                hash *= 16777619u;
            }
        return hash != 0 ? hash : 1u;
    }

    void updateFuel(const Sample &s)
    {
        if (isfinite(s.fuelFraction))
        {
            // Refuelled: the reserve warning can come again.
            if (isfinite(fuelLowest) && s.fuelFraction > fuelLowest + 0.05f) { fuelArmed = true; fuelLowest = s.fuelFraction; }
            if (!isfinite(fuelLowest) || s.fuelFraction < fuelLowest) fuelLowest = s.fuelFraction;
        }
        const bool low = (isfinite(s.fuelLaps) && s.fuelLaps >= 0 && s.fuelLaps < 1.5f) ||
            (isfinite(s.fuelFraction) && s.fuelFraction < 0.08f);
        if (low && fuelArmed)
        {
            fuelArmed = false;
            push(EventType::LowFuel, -1, -1);
        }
    }
};

// Formats a time in milliseconds as m:ss.mmm (or ss.mmm under a minute).
inline void formatLapTime(int32_t ms, char *out, size_t size, bool tenths = false)
{
    if (ms < 0) { snprintf(out, size, tenths ? "-:--.-" : "-:--.---"); return; }
    const int minutes = ms / 60000;
    const int seconds = (ms / 1000) % 60;
    const int millis = ms % 1000;
    if (tenths) snprintf(out, size, "%d:%02d.%d", minutes, seconds, millis / 100);
    else snprintf(out, size, "%d:%02d.%03d", minutes, seconds, millis);
}
} // namespace LapAnalysis
