// Tests of src/LapAnalysis.h on a simulated stadium circuit:
//   g++ -std=c++11 -Wall -Wextra -pedantic tests/lap_analysis.cpp -o /tmp/lap-tests && /tmp/lap-tests
#include "../src/LapAnalysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <functional>

using namespace LapAnalysis;

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
    } while (0)
#define CHECK_NEAR(value, expected, tolerance)                                   \
    do {                                                                         \
        const double v_ = (value), e_ = (expected);                              \
        if (!(fabs(v_ - e_) <= (tolerance))) {                                   \
            printf("FAIL %s:%d: %s = %g, expected %g +- %g\n", __FILE__, __LINE__, #value, v_, e_, (double)(tolerance)); \
            failures++;                                                          \
        }                                                                        \
    } while (0)

// Stadium circuit drawn clockwise on screen (x right, z down): 500 m straight
// along x at z = 0, right-hand semicircle of radius 100 m, straight back at
// z = 200 and a left semicircle. The line is at (0, 0).
static const float STRAIGHT = 500.0f, RADIUS = 100.0f;
static const float PI_F = 3.14159265f;
static const float LENGTH = 2 * STRAIGHT + 2 * PI_F * RADIUS;

static void pointAt(float s, float &x, float &z, float &hx, float &hz)
{
    s = fmodf(s, LENGTH);
    if (s < STRAIGHT) { x = s; z = 0; hx = 1; hz = 0; return; }
    s -= STRAIGHT;
    if (s < PI_F * RADIUS)
    {
        const float a = s / RADIUS; // angle travelled on the right turn
        x = STRAIGHT + RADIUS * sinf(a); z = RADIUS - RADIUS * cosf(a);
        hx = cosf(a); hz = sinf(a);
        return;
    }
    s -= PI_F * RADIUS;
    if (s < STRAIGHT) { x = STRAIGHT - s; z = 2 * RADIUS; hx = -1; hz = 0; return; }
    s -= STRAIGHT;
    const float a = s / RADIUS;
    x = -RADIUS * sinf(a); z = RADIUS + RADIUS * cosf(a);
    hx = -cosf(a); hz = -sinf(a);
}

// Drives the circuit at 60 Hz like GT7 does (or AC with the lap fraction).
struct Driver
{
    Analyzer &analyzer;
    bool fractionMode = false;
    bool sendPositions = true;
    float s = 0;          // distance along the circuit
    double time = 0;      // seconds
    int lapCount = 0;
    int totalLaps = 0;
    double lapStart = 0;  // time of the last line crossing
    int32_t lastLapMs = -1;
    bool lapStarted = false;
    float offsetX = 0, offsetZ = 0;
    float fuel = NAN;
    bool skipNextCrossing = false; // race start: lap 1 began on the grid

    explicit Driver(Analyzer &a) : analyzer(a) {}

    Sample sample(float speed, float throttle = 0.5f, float brake = 0)
    {
        Sample out;
        out.timeMs = static_cast<uint32_t>(llround(time * 1000.0));
        out.speed = speed;
        float x, z, hx, hz;
        pointAt(s, x, z, hx, hz);
        out.hasPosition = sendPositions;
        out.x = x + offsetX; out.z = z + offsetZ;
        out.hasVelocity = !fractionMode;
        out.vx = hx * speed; out.vz = hz * speed;
        out.lapCount = lapCount;
        out.totalLaps = totalLaps;
        out.lapTimeMs = lapStarted ? static_cast<int32_t>(llround((time - lapStart) * 1000.0)) : 0;
        out.lastLapMs = lastLapMs;
        if (fractionMode) out.lapFraction = fmodf(s, LENGTH) / LENGTH;
        out.throttle = throttle;
        out.brake = brake;
        out.fuelFraction = fuel;
        return out;
    }

    // Drives `distance` metres with a speed that depends on the position.
    void drive(float distance, const std::function<float(float)> &speedAt)
    {
        const double dt = 1.0 / 60.0;
        const float end = s + distance;
        while (s < end)
        {
            const float v = speedAt(fmodf(s, LENGTH));
            const float before = s;
            s += v * dt;
            time += dt;
            if (floorf(before / LENGTH) != floorf(s / LENGTH) && skipNextCrossing)
                skipNextCrossing = false;
            else if (floorf(before / LENGTH) != floorf(s / LENGTH))
            {
                // Exact crossing time of the line.
                const double over = (s - floorf(s / LENGTH) * LENGTH) / v;
                const double crossing = time - over;
                if (lapStarted) lastLapMs = static_cast<int32_t>(llround((crossing - lapStart) * 1000.0));
                lapStart = crossing;
                lapStarted = true;
                lapCount++;
            }
            analyzer.update(sample(v));
        }
    }

    void driveLaps(float laps, float speed)
    {
        drive(laps * LENGTH, [speed](float) { return speed; });
    }
};

static int eventsOf(Analyzer &a, EventType type, Event *last = nullptr)
{
    int count = 0;
    Event e;
    while (a.takeEvent(e))
        if (e.type == type) { count++; if (last) *last = e; }
    return count;
}

static Analyzer *newAnalyzer() { return new Analyzer(); }

static void testLapsDeltaAndMap()
{
    Analyzer &a = *newAnalyzer();
    Driver d(a);
    d.s = 800; // join on the bottom straight: out-lap, not timed
    d.driveLaps(1.0f - 800.0f / LENGTH + 0.01f, 40.0f);
    CHECK(d.lapCount == 1);
    CHECK(a.mapState() == MapState::Recording);
    CHECK(!a.hasLiveDelta());
    CHECK(a.lapsCompleted() == 0);
    CHECK(eventsOf(a, EventType::BestLap) == 0);

    // Lap 1 at 40 m/s: reference, map and sectors.
    d.driveLaps(1.0f, 40.0f);
    CHECK(d.lapCount == 2);
    CHECK(a.lapsCompleted() == 1);
    CHECK_NEAR(a.lapRecord(0).lapMs, LENGTH / 40.0f * 1000.0f, 20);
    CHECK_NEAR(a.referenceLapMs(), LENGTH / 40.0f * 1000.0f, 20);
    Event best;
    CHECK(eventsOf(a, EventType::BestLap, &best) == 1);
    CHECK(best.gainMs == -1);
    CHECK(a.mapState() == MapState::Complete);
    CHECK(a.mapPointCount() == MAP_POINTS);
    float minX, minZ, maxX, maxZ;
    CHECK(a.mapBounds(minX, minZ, maxX, maxZ));
    CHECK_NEAR(minX, -RADIUS, 3);
    CHECK_NEAR(maxX, STRAIGHT + RADIUS, 3);
    CHECK_NEAR(minZ, 0, 3);
    CHECK_NEAR(maxZ, 2 * RADIUS, 3);
    float x0, z0;
    a.mapPoint(0, x0, z0);
    CHECK_NEAR(x0, 0, 2); // the map starts at the line
    CHECK_NEAR(z0, 0, 2);
    CHECK(a.sectorsAvailable());

    // Lap 2 at 41 m/s: negative delta growing along the lap, purple sectors.
    d.drive(800.0f, [](float) { return 41.0f; });
    CHECK(a.hasLiveDelta());
    CHECK_NEAR(a.liveDeltaMs(), (800.0f / 41.0f - 800.0f / 40.0f) * 1000.0f, 25);
    CHECK(a.sectorState(0) == SectorState::Purple);
    CHECK(a.currentSector() == 1);
    CHECK(a.sectorState(1) == SectorState::Running);
    CHECK_NEAR(a.lapProgress(), 800.0f / LENGTH, 0.02);
    d.drive(LENGTH - 800.0f, [](float) { return 41.0f; });
    CHECK(a.lapsCompleted() == 2);
    CHECK(eventsOf(a, EventType::BestLap, &best) == 1);
    CHECK_NEAR(best.gainMs, (LENGTH / 40.0f - LENGTH / 41.0f) * 1000.0f, 25);
    for (int i = 0; i < SECTORS; ++i) CHECK(a.lastLapSectorState(i) == SectorState::Purple);
    CHECK_NEAR(a.referenceLapMs(), LENGTH / 41.0f * 1000.0f, 20);

    // Lap 3 at 39 m/s: positive delta, yellow sectors, no best lap.
    d.drive(1000.0f, [](float) { return 39.0f; });
    CHECK(a.hasLiveDelta());
    CHECK_NEAR(a.liveDeltaMs(), (1000.0f / 39.0f - 1000.0f / 41.0f) * 1000.0f, 25);
    d.drive(LENGTH - 1000.0f, [](float) { return 39.0f; });
    CHECK(eventsOf(a, EventType::BestLap) == 0);
    for (int i = 0; i < SECTORS; ++i) CHECK(a.lastLapSectorState(i) == SectorState::Yellow);
    CHECK(a.lapsCompleted() == 3);
    CHECK_NEAR(a.bestLapMs(), LENGTH / 41.0f * 1000.0f, 20);
    CHECK_NEAR(a.averageLapMs(), (LENGTH / 40 + LENGTH / 41 + LENGTH / 39) / 3 * 1000.0f, 30);
    CHECK_NEAR(a.topSpeedKmh(), 41.0f * 3.6f, 0.1);

    // Sector colours: faster than the best lap's sector but not the session
    // best is green. Session bests per sector now come from lap 2 (41 m/s).
    const float third = LENGTH / 3;
    d.drive(LENGTH, [third](float s) { return s < third ? 40.5f : s < 2 * third ? 42.0f : 38.0f; });
    // Sector 1 slower than lap 2 (yellow), sector 2 faster (purple), 3 yellow.
    CHECK(a.lastLapSectorState(0) == SectorState::Yellow);
    CHECK(a.lastLapSectorState(1) == SectorState::Purple);
    CHECK(a.lastLapSectorState(2) == SectorState::Yellow);
    delete &a;
}

static void testGreenSector()
{
    Analyzer &a = *newAnalyzer();
    Driver d(a);
    d.drive(LENGTH * 0.5f, [](float) { return 40.0f; }); // out-lap
    const float third = LENGTH / 3;
    auto lap = [&](float v1, float v2, float v3) {
        d.drive(LENGTH, [=](float s) { return s < third ? v1 : s < 2 * third ? v2 : v3; });
    };
    d.drive(LENGTH * 0.5f + 1, [](float) { return 40.0f; });
    lap(40, 40, 40);  // reference: 3 x 13.57 s
    lap(42, 38, 38.5f); // sector 1 best (purple), slower lap: reference unchanged
    lap(41, 41, 38);  // sector 1: slower than 42 (best) but faster than 40 (reference)
    CHECK(a.lastLapSectorState(0) == SectorState::Green);
    CHECK(a.lastLapSectorState(1) == SectorState::Purple);
    CHECK(a.lastLapSectorState(2) == SectorState::Yellow);
    delete &a;
}

static void testLateralG()
{
    Analyzer &a = *newAnalyzer();
    Driver d(a);
    d.drive(STRAIGHT * 0.8f, [](float) { return 30.0f; });
    CHECK_NEAR(a.lateralG(), 0, 0.05);
    // Middle of the right-hand turn: v^2 / r towards the right.
    d.drive(STRAIGHT * 0.2f + PI_F * RADIUS * 0.5f, [](float) { return 30.0f; });
    CHECK_NEAR(a.lateralG(), 30.0f * 30.0f / RADIUS / GRAVITY, 0.08);
    CHECK(a.gTrailCount() > 10);
    float lat, lon;
    a.gTrail(0, lat, lon);
    CHECK(lat > 0.8f);
    CHECK_NEAR(lon, 0, 0.05);
    CHECK_NEAR(a.peakLateralG(), 30.0f * 30.0f / RADIUS / GRAVITY, 0.1);
    delete &a;

    // Same turn without velocity vector (Assetto Corsa): heading from positions.
    Analyzer &b = *newAnalyzer();
    Driver e(b);
    e.fractionMode = true;
    e.drive(STRAIGHT + PI_F * RADIUS * 0.5f, [](float) { return 30.0f; });
    CHECK_NEAR(b.lateralG(), 30.0f * 30.0f / RADIUS / GRAVITY, 0.12);
    delete &b;
}

static void testLaunch()
{
    Analyzer &a = *newAnalyzer();
    Driver d(a);
    // Stopped on the straight for 4 s: staged, then the tree runs to green.
    for (int i = 0; i < 240; ++i) { d.time += 1.0 / 60; a.update(d.sample(0, 0, 0.5f)); }
    CHECK(a.launchPhase() == LaunchPhase::Staged);
    CHECK(a.treeLight() == TreeLight::Green);
    // Constant 5 m/s^2 from rest.
    const double accel = 5.0;
    double t = 0, v = 0;
    while (v < 230 / 3.6)
    {
        t += 1.0 / 60;
        v = accel * t;
        d.s += static_cast<float>(v / 60);
        d.time += 1.0 / 60;
        a.update(d.sample(static_cast<float>(v), 1, 0));
    }
    const LaunchRun &run = a.currentRun();
    CHECK(a.launchPhase() == LaunchPhase::Running);
    CHECK_NEAR(run.t100, 100 / 3.6 / accel, 0.02);
    CHECK_NEAR(run.t200, 200 / 3.6 / accel, 0.02);
    CHECK_NEAR(run.t100to200, 100 / 3.6 / accel, 0.03);
    CHECK_NEAR(run.t400, sqrt(2 * 400 / accel), 0.03);
    CHECK_NEAR(run.v400Kmh, sqrt(2 * 400 * accel) * 3.6, 0.5);
    CHECK(run.reaction > 0); // started after the green light
    // Braking ends the run and keeps the results.
    d.time += 1.0 / 60;
    a.update(d.sample(static_cast<float>(v - 1), 0, 0.8f));
    CHECK(a.launchPhase() == LaunchPhase::Finished);
    CHECK_NEAR(a.bestRuns().t100, 100 / 3.6 / accel, 0.02);
    CHECK_NEAR(a.bestRuns().vmaxKmh, 230, 1.5);
    delete &a;
}

static void testRewindAndTrackChange()
{
    Analyzer &a = *newAnalyzer();
    Driver d(a);
    d.drive(LENGTH * 0.5f, [](float) { return 40.0f; });
    d.drive(LENGTH * 0.5f + 1, [](float) { return 40.0f; }); // crosses the line: lap 1
    d.drive(600, [](float) { return 40.0f; });
    CHECK(a.mapState() == MapState::Recording);
    // Rewind: back 300 m and 7.5 s.
    d.s -= 300;
    d.lapStart += 7.5;
    d.drive(LENGTH - 300 + 1, [](float) { return 40.0f; });
    CHECK(a.lapsCompleted() == 0); // the broken lap is not timed
    CHECK(a.mapState() == MapState::Recording); // recording again from the line
    d.driveLaps(1, 40);
    CHECK(a.lapsCompleted() == 1);
    CHECK(a.mapState() == MapState::Complete);

    // Another circuit 5 km away: after a while everything starts over.
    d.offsetX = 5000;
    d.driveLaps(1.2f, 40);
    CHECK(a.mapState() != MapState::Complete);
    CHECK(a.lapsCompleted() == 0);
    CHECK(a.referenceLapMs() < 0);
    delete &a;
}

static void testOfficialTimeAndFinalLap()
{
    Analyzer &a = *newAnalyzer();
    Driver d(a);
    d.totalLaps = 3;
    d.drive(LENGTH * 0.5f, [](float) { return 40.0f; });
    d.drive(LENGTH * 0.5f + 1, [](float) { return 40.0f; });
    eventsOf(a, EventType::FinalLap);
    d.driveLaps(1, 40); // lap 2 starts
    CHECK(eventsOf(a, EventType::FinalLap) == 0);
    d.driveLaps(1, 40); // lap 3 = last lap
    Event e;
    bool final = false;
    while (a.takeEvent(e)) if (e.type == EventType::FinalLap) final = true;
    CHECK(final);

    // Official lap time published a few packets after the line.
    Analyzer &b = *newAnalyzer();
    Driver late(b);
    late.drive(LENGTH * 0.5f, [](float) { return 40.0f; });
    late.drive(LENGTH * 0.5f + 1, [](float) { return 40.0f; });
    late.drive(LENGTH - 5, [](float) { return 40.0f; });
    CHECK(late.lapCount == 1);
    const int32_t official = 41234;
    // Cross the line with a stale official time, then publish it.
    Sample s;
    for (int i = 0; i < 20; ++i)
    {
        late.time += 1.0 / 60;
        late.s += 40.0f / 60;
        s = late.sample(40);
        if (late.s >= 2 * LENGTH && late.lapCount == 1) { late.lapCount = 2; late.lapStart = late.time; s = late.sample(40); }
        s.lastLapMs = late.lapCount == 2 && i >= 14 ? official : -1;
        b.update(s);
    }
    CHECK(b.lapsCompleted() == 1);
    CHECK(b.lapRecord(0).lapMs == official);
    CHECK(b.bestLapMs() == official);
    delete &a;
    delete &b;
}

static void testFractionMode()
{
    // Assetto Corsa: lap fraction, positions, no velocity; join mid-lap.
    Analyzer &a = *newAnalyzer();
    Driver d(a);
    d.fractionMode = true;
    d.s = 400;
    d.drive(LENGTH - 400 + 1, [](float) { return 40.0f; });
    CHECK(a.sectorsAvailable()); // the fraction gives the sectors from the start
    d.driveLaps(1, 40);
    CHECK(a.lapsCompleted() == 1);
    CHECK(a.mapState() == MapState::Complete);
    d.drive(500, [](float) { return 42.0f; });
    CHECK(a.hasLiveDelta());
    CHECK_NEAR(a.liveDeltaMs(), (500 / 42.0f - 500 / 40.0f) * 1000.0f, 40);
    d.drive(LENGTH - 500, [](float) { return 42.0f; });
    CHECK_NEAR(a.referenceLapMs(), LENGTH / 42.0f * 1000.0f, 25);
    // Near the end of the next lap the delta still compares the same point.
    d.drive(LENGTH - 30, [](float) { return 40.0f; });
    CHECK(a.hasLiveDelta());
    CHECK_NEAR(a.liveDeltaMs(), ((LENGTH - 30) / 40.0f - (LENGTH - 30) / 42.0f) * 1000.0f, 60);
    delete &a;
}

static void testLowFuel()
{
    Analyzer &a = *newAnalyzer();
    Driver d(a);
    d.fuel = 0.2f;
    d.driveLaps(0.2f, 40);
    CHECK(eventsOf(a, EventType::LowFuel) == 0);
    d.fuel = 0.07f;
    d.driveLaps(0.1f, 40);
    CHECK(eventsOf(a, EventType::LowFuel) == 1);
    d.fuel = 0.05f;
    d.driveLaps(0.1f, 40);
    CHECK(eventsOf(a, EventType::LowFuel) == 0); // once per stint
    d.fuel = 1.0f;                               // pit stop
    d.driveLaps(0.1f, 40);
    d.fuel = 0.06f;
    d.driveLaps(0.1f, 40);
    CHECK(eventsOf(a, EventType::LowFuel) == 1);
    delete &a;
}

static void testStandingStartAndCarChange()
{
    // GT7 race: the counter goes to 1 with the car stopped on the grid,
    // 150 m before the line.
    Analyzer &a = *newAnalyzer();
    Driver d(a);
    d.s = LENGTH - 150;
    d.lapCount = 0;
    for (int i = 0; i < 120; ++i) { d.time += 1.0 / 60; a.update(d.sample(0)); }
    d.lapCount = 1;
    d.lapStart = d.time;
    d.lapStarted = true;
    for (int i = 0; i < 60; ++i) { d.time += 1.0 / 60; a.update(d.sample(0)); }
    d.skipNextCrossing = true;
    d.drive(150 + 5, [](float) { return 30.0f; }); // crosses the line, still lap 1
    CHECK(d.lapCount == 1);
    CHECK(a.mapState() == MapState::Waiting);
    CHECK(a.currentSector() == -1);
    d.drive(LENGTH - 5, [](float) { return 40.0f; }); // end of lap 1: lap 2 starts
    CHECK(d.lapCount == 2);
    CHECK(a.lapsCompleted() == 1);       // timed...
    CHECK(a.referenceLapMs() < 0);       // ...but not a reference
    CHECK(a.mapState() == MapState::Recording); // the map starts with the flying lap
    d.driveLaps(1, 40);
    CHECK(a.mapState() == MapState::Complete);
    CHECK(a.referenceLapMs() > 0);
    CHECK(a.sectorsAvailable());

    // Another car: times start over, the map stays.
    a.resetTiming();
    CHECK(a.lapsCompleted() == 0);
    CHECK(a.referenceLapMs() < 0);
    CHECK(a.bestLapMs() < 0);
    CHECK(a.mapState() == MapState::Complete);
    d.driveLaps(1, 38);
    CHECK(a.lapsCompleted() == 1);
    CHECK_NEAR(a.referenceLapMs(), LENGTH / 38.0f * 1000.0f, 25);
    for (int i = 0; i < SECTORS; ++i) CHECK(a.lastLapSectorState(i) == SectorState::Purple);
    delete &a;
}

static void testFormat()
{
    char buffer[16];
    formatLapTime(84382, buffer, sizeof(buffer));
    CHECK(strcmp(buffer, "1:24.382") == 0);
    formatLapTime(-1, buffer, sizeof(buffer));
    CHECK(strcmp(buffer, "-:--.---") == 0);
    formatLapTime(9050, buffer, sizeof(buffer), true);
    CHECK(strcmp(buffer, "0:09.0") == 0);
}

int main()
{
    testLapsDeltaAndMap();
    testGreenSector();
    testLateralG();
    testLaunch();
    testRewindAndTrackChange();
    testOfficialTimeAndFinalLap();
    testFractionMode();
    testLowFuel();
    testStandingStartAndCarChange();
    testFormat();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    printf("lap analysis: all tests passed\n");
    return 0;
}
