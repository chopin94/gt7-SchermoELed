#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

class Analyzer
{
public:
    Analyzer() { reset(); }

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
    }

    void update(const Sample &s)
    {
        lastTimeMs = s.timeMs;
        if (!s.driving)
        {
            // Paused or in the menus: nothing moves. The next driving sample
            // restarts the derivatives instead of seeing a long time step.
            motionGap = true;
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
    int mapPointCount() const { return map.state == MapState::Complete ? MAP_POINTS : map.rawCount; }
    void mapPoint(int i, float &x, float &z) const
    {
        if (map.state == MapState::Complete) { x = map.x[i]; z = map.z[i]; }
        else { x = map.rawX[i]; z = map.rawZ[i]; }
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
    };

    struct MapData
    {
        MapState state = MapState::Waiting;
        float rawX[MAP_RAW_POINTS];
        float rawZ[MAP_RAW_POINTS];
        int rawCount = 0;
        float spacing = MAP_FIRST_SPACING_M;
        float lastRawDistance = 0;
        float x[MAP_POINTS];
        float z[MAP_POINTS];
        float minX = 0, minZ = 0, maxX = 0, maxZ = 0;
    };

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

    uint32_t curTime[DELTA_CELLS];
    uint32_t refTime[DELTA_CELLS];
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

    Event events[EVENT_QUEUE];
    int eventHead = 0, eventCount = 0;

    // ---- helpers ----
    static bool valid(int sector) { return sector >= 0 && sector < SECTORS; }
    static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
    int traceIndex(int age) const { return (traceHead - 1 - age + 2 * TRACE_SAMPLES) % TRACE_SAMPLES; }

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
        if (s.lapCount == previous.lapCount && s.lapTimeMs >= 0 && lap.lastTime >= 0 &&
            s.lapTimeMs + 500 < lap.lastTime)
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
        lap = LapState();
        lap.observed = fromLine;
        // A race starts from the grid, behind the line: that lap is timed but
        // its distances are shifted, so it gives no map, reference or sectors.
        lap.standingStart = fromLine && s.speed < 5.0f && !isfinite(s.lapFraction);
        lap.continuous = true;
        lap.lastTime = s.lapTimeMs;
        lap.progressKnown = fromLine || isfinite(s.lapFraction);
        if (lap.progressKnown)
        {
            lap.progress = progressCells(s);
            lap.lastProgress = lap.progress;
            if (fromLine)
            {
                // The cells up to here belong to the line.
                const uint32_t t = s.lapTimeMs >= 0 ? static_cast<uint32_t>(s.lapTimeMs) : 0;
                const int cells = static_cast<int>(floorf(lap.progress));
                for (int c = 0; c <= cells && c < DELTA_CELLS; ++c) curTime[c] = t;
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
                curTime[c] = static_cast<uint32_t>(lap.lastTime + (lapMs - lap.lastTime) * clampf(k, 0, 1));
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
            memcpy(refTime, curTime, sizeof(uint32_t) * refCells);
            if (!lapFractionMode) lapCells = refCells;
            for (int i = 0; i < SECTORS; ++i) refSector[i] = lap.sectorMs[i];
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
        return static_cast<int32_t>(curTime[c] + (static_cast<float>(curTime[c + 1]) - curTime[c]) * f);
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
        if (!lap.progressKnown || s.lapTimeMs < 0) { lap.lastTime = s.lapTimeMs; return; }

        const float p = progressCells(s);
        const int32_t t = s.lapTimeMs;
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
                curTime[c] = static_cast<uint32_t>(t0 + (t - t0) * k);
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
                const float ref = refTime[c] + (static_cast<float>(refTime[c + 1]) - refTime[c]) * f;
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
                map.rawX[i] = map.rawX[i * 2];
                map.rawZ[i] = map.rawZ[i * 2];
            }
            map.rawCount = MAP_RAW_POINTS / 2;
            map.spacing *= 2;
        }
        if (map.rawCount == 0) { map.minX = map.maxX = x; map.minZ = map.maxZ = z; }
        map.rawX[map.rawCount] = x;
        map.rawZ[map.rawCount] = z;
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
            total += hypotf(map.rawX[j] - map.rawX[i], map.rawZ[j] - map.rawZ[i]);
        }
        if (n < 24 || total < 300.0f) { map.state = MapState::Waiting; map.rawCount = 0; mapRev++; return; }

        const float step = total / MAP_POINTS;
        int segment = 0;
        float segmentStart = 0;
        float segmentLength = hypotf(map.rawX[1 % n] - map.rawX[0], map.rawZ[1 % n] - map.rawZ[0]);
        for (int k = 0; k < MAP_POINTS; ++k)
        {
            const float target = k * step;
            while (segmentStart + segmentLength < target && segment < n - 1)
            {
                segmentStart += segmentLength;
                segment++;
                const int next = (segment + 1) % n;
                segmentLength = hypotf(map.rawX[next] - map.rawX[segment], map.rawZ[next] - map.rawZ[segment]);
            }
            const int next = (segment + 1) % n;
            const float f = segmentLength > 0 ? clampf((target - segmentStart) / segmentLength, 0, 1) : 0;
            map.x[k] = map.rawX[segment] + (map.rawX[next] - map.rawX[segment]) * f;
            map.z[k] = map.rawZ[segment] + (map.rawZ[next] - map.rawZ[segment]) * f;
        }
        map.minX = map.maxX = map.x[0];
        map.minZ = map.maxZ = map.z[0];
        for (int k = 1; k < MAP_POINTS; ++k)
        {
            if (map.x[k] < map.minX) map.minX = map.x[k];
            if (map.x[k] > map.maxX) map.maxX = map.x[k];
            if (map.z[k] < map.minZ) map.minZ = map.z[k];
            if (map.z[k] > map.maxZ) map.maxZ = map.z[k];
        }
        map.state = MapState::Complete;
        mapRev++;
    }

    // Far from the recorded map for a while: another circuit, start over.
    void checkTrackChange(const Sample &s)
    {
        if (map.state != MapState::Complete || !s.hasPosition || s.speed < 5.0f) return;
        if (++trackCheckCounter % 15 != 0) return;
        float best = 1e12f;
        for (int k = 0; k < MAP_POINTS; ++k)
        {
            const float dx = map.x[k] - s.x, dz = map.z[k] - s.z;
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
