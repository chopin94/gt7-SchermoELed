// Tests of the braking points in src/LapAnalysis.h:
//   g++ -std=c++11 -Wall -Wextra -pedantic tests/brake_points.cpp -o /tmp/brake-tests && /tmp/brake-tests
#include "stadium.h"
#include <stdio.h>
#include <stdlib.h>

using namespace LapAnalysis;
using namespace stadium;

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

// Where the nominal braking points are: 366 m along each straight.
static const float ZONE1_X = 366.0f, ZONE1_Z = 0.0f;
static const float ZONE2_X = STRAIGHT - 366.0f, ZONE2_Z = 2 * RADIUS;

// Reference lap: the first complete lap, braking at the nominal points.
static void recordReference(Driver &d)
{
    d.s = 1400; // join in the second corner: an out-lap, then lap 1 is timed
    d.driveToLap(2);
}

static void testRecordsTheReferenceZones()
{
    Analyzer &a = *new Analyzer();
    Driver d(a);
    CHECK(a.referenceBrakeZoneCount() == 0);
    float meters, kmh;
    CHECK(!a.nextBrakePoint(meters, kmh));
    recordReference(d);
    CHECK(a.referenceBrakeZoneCount() == 2);
    BrakeZone z;
    CHECK(a.referenceBrakeZone(0, z));
    CHECK_NEAR(z.x, ZONE1_X, 3.0); CHECK_NEAR(z.z, ZONE1_Z, 1.0);
    CHECK_NEAR(z.hx, 1.0, 0.02);
    CHECK_NEAR(z.entryKmh, 60.0 * 3.6, 5.0);
    CHECK(z.peak > 0.8f);
    CHECK(a.referenceBrakeZone(1, z));
    CHECK_NEAR(z.x, ZONE2_X, 3.0); CHECK_NEAR(z.z, ZONE2_Z, 1.0);
    CHECK_NEAR(z.hx, -1.0, 0.02);
    CHECK(!a.referenceBrakeZone(2, z));
    CHECK(!a.referenceBrakeZone(-1, z));
    // Nothing to compare with yet.
    float delta, entry, reference;
    CHECK(a.lastBrakeResult(delta, entry, reference));
    CHECK(!isfinite(delta) && !isfinite(reference));
    CHECK_NEAR(entry, 216.0, 5.0);
    delete &a;
}

static void testEarlierAndLaterThanTheReference()
{
    Analyzer &a = *new Analyzer();
    Driver d(a);
    recordReference(d);

    // Lap 2: the pedal goes down 20 m before the reference point.
    d.earlier = 20;
    d.driveToLap(3);
    float delta, entry, reference;
    CHECK(a.lastBrakeResult(delta, entry, reference));
    CHECK_NEAR(delta, 20.0, 2.5);
    CHECK_NEAR(reference, 216.0, 5.0);
    CHECK_NEAR(a.lastLapBrakeDelta(0), 20.0, 2.5);
    CHECK_NEAR(a.lastLapBrakeDelta(1), 20.0, 2.5);
    CHECK(!isfinite(a.brakeDelta(0))); // new lap: nothing braked yet
    // A slower lap does not take the reference over.
    BrakeZone z;
    CHECK(a.referenceBrakeZone(0, z));
    CHECK_NEAR(z.x, ZONE1_X, 3.0);

    // Lap 3: 8 m later than the reference, and faster.
    d.earlier = -8;
    d.driveTo(500);
    CHECK_NEAR(a.brakeDelta(0), -8.0, 2.5);
    CHECK(!isfinite(a.brakeDelta(1)));
    CHECK(a.lastBrakeIndex() == 0);
    d.driveToLap(4);
    // The faster lap is the new reference: its braking points are the ones to beat.
    CHECK(a.referenceBrakeZone(0, z));
    CHECK_NEAR(z.x, ZONE1_X + 8.0, 3.0);
    CHECK(a.referenceBrakeZone(1, z));
    CHECK_NEAR(z.x, ZONE2_X - 8.0, 3.0);
    // Results against the old reference are void.
    CHECK(!isfinite(a.lastLapBrakeDelta(0)) && !isfinite(a.lastLapBrakeDelta(1)));
    delete &a;
}

static void testDistanceToTheNextPoint()
{
    Analyzer &a = *new Analyzer();
    Driver d(a);
    recordReference(d);
    d.driveToLap(3);
    float meters = 0, kmh = 0;
    d.driveTo(100);
    CHECK(a.nextBrakeIndex() == 0);
    CHECK(a.nextBrakePoint(meters, kmh));
    CHECK_NEAR(meters, ZONE1_X - 100.0, 6.0);
    CHECK_NEAR(kmh, 216.0, 5.0);
    d.driveTo(300);
    CHECK(a.nextBrakePoint(meters, kmh));
    CHECK_NEAR(meters, ZONE1_X - 300.0, 6.0);
    // Past the point, with the pedal down: the next one is on the bottom straight.
    d.driveTo(450);
    CHECK(a.nextBrakeIndex() == 1);
    d.driveTo(600);
    CHECK(a.nextBrakeIndex() == 1);
    CHECK(!a.nextBrakePoint(meters, kmh)); // in the corner it is behind the car
    d.driveTo(900);
    CHECK(a.nextBrakePoint(meters, kmh));
    CHECK_NEAR(meters, (STRAIGHT + TURN + 366.0f) - 900.0f, 6.0); // along the bottom straight
    d.driveTo(1250);
    CHECK(a.nextBrakeIndex() == -1); // both points are behind
    // A new lap starts again from the first point.
    d.driveToLap(4);
    d.drive(20);
    CHECK(a.nextBrakeIndex() == 0);
    delete &a;
}

static void testSkippedPointIsPassed()
{
    Analyzer &a = *new Analyzer();
    Driver d(a);
    recordReference(d);
    d.driveToLap(3);
    // Lift off and coast through the first corner without braking.
    d.pace = 0.0f; // handled below by driving a flat profile
    d.pace = 1.0f;
    // Instead of a profile with a zone: keep the speed at the corner speed.
    Driver slow(a);
    slow.s = d.s; slow.time = d.time; slow.lapCount = d.lapCount; slow.lapStart = d.lapStart;
    slow.lapStarted = true; slow.lastLapMs = d.lastLapMs;
    slow.pace = 1.0f;
    slow.earlier = 366.0f - 1.0f; // brakes right away at the exit: no zone on the straight
    slow.driveTo(480);
    CHECK(a.nextBrakeIndex() == 1); // passed the first point without braking
    CHECK(!isfinite(a.brakeDelta(0)));
    delete &a;
}

// A cruising car on a straight line along x, to try single pedal actions.
struct Straight
{
    Analyzer &a;
    uint32_t ms = 1000;
    float x = 0, speed = 50.0f;
    int lap = 0;
    explicit Straight(Analyzer &analyzer) : a(analyzer) {}
    void step(float brake, float decel = 0.0f)
    {
        Sample s;
        s.timeMs = ms;
        s.speed = speed;
        s.hasPosition = true;
        s.x = x; s.z = 0;
        s.hasVelocity = true;
        s.vx = speed; s.vz = 0;
        s.lapCount = lap;
        s.lapTimeMs = static_cast<int32_t>(ms - 1000);
        s.brake = brake;
        a.update(s);
        ms += 17;
        speed -= decel * 0.017f;
        if (speed < 0) speed = 0;
        x += speed * 0.017f;
    }
    void run(float seconds, float brake, float decel = 0.0f)
    {
        const int n = static_cast<int>(seconds / 0.017f);
        for (int i = 0; i < n; ++i) step(brake, decel);
    }
};

static void testWhatIsNotABrakingZone()
{
    {   // A brush of the pedal.
        Analyzer &a = *new Analyzer();
        Straight c(a);
        c.run(1.0f, 0);
        c.run(0.2f, 0.3f);
        c.run(2.0f, 0);
        CHECK(a.brakeZoneCount() == 0);
        delete &a;
    }
    {   // Resting a foot on the pedal: it does not slow the car.
        Analyzer &a = *new Analyzer();
        Straight c(a);
        c.run(1.0f, 0);
        c.run(3.0f, 0.3f, 0.0f);
        c.run(1.0f, 0);
        CHECK(a.brakeZoneCount() == 0);
        delete &a;
    }
    {   // Slow: under the minimum speed.
        Analyzer &a = *new Analyzer();
        Straight c(a);
        c.speed = 15.0f;
        c.run(1.0f, 0);
        c.run(1.0f, 0.9f, 8.0f);
        CHECK(a.brakeZoneCount() == 0);
        delete &a;
    }
    {   // A real braking.
        Analyzer &a = *new Analyzer();
        Straight c(a);
        c.run(1.0f, 0);
        c.run(1.5f, 0.9f, 12.0f);
        c.run(1.0f, 0);
        CHECK(a.brakeZoneCount() == 1);
        float delta, entry, ref;
        CHECK(a.lastBrakeResult(delta, entry, ref));
        CHECK_NEAR(entry, 180.0, 2.0);
        delete &a;
    }
}

static void testPulsingIsOneZone()
{
    Analyzer &a = *new Analyzer();
    Straight c(a);
    c.run(1.0f, 0);
    // The pedal is released for 0.3 s in the middle: still the same braking.
    c.run(0.6f, 0.9f, 12.0f);
    c.run(0.3f, 0, 4.0f);
    c.run(0.6f, 0.9f, 12.0f);
    c.run(1.0f, 0);
    CHECK(a.brakeZoneCount() == 1);
    // A second pressing 0.6 s after the end of the first: the same corner, no new zone.
    c.speed = 50.0f;
    c.run(1.0f, 0.9f, 12.0f);
    c.run(0.6f, 0);
    CHECK(a.brakeZoneCount() == 1);
    // Much later: another corner.
    c.speed = 50.0f;
    c.run(3.0f, 0);
    c.run(1.5f, 0.9f, 12.0f);
    CHECK(a.brakeZoneCount() == 2);
    delete &a;
}

static void testRewindFindsTheNextPointAgain()
{
    Analyzer &a = *new Analyzer();
    Driver d(a);
    recordReference(d);
    d.driveToLap(3);
    d.driveTo(250);
    CHECK(a.nextBrakeIndex() == 0);
    // A rewind brings the car back on the bottom straight.
    d.s += 650.0f;
    d.drive(30);
    CHECK(a.nextBrakeIndex() == 1);
    float meters, kmh;
    CHECK(a.nextBrakePoint(meters, kmh));
    delete &a;
}

static void testResets()
{
    Analyzer &a = *new Analyzer();
    Driver d(a);
    recordReference(d);
    CHECK(a.referenceBrakeZoneCount() == 2);
    a.resetTiming(); // another car: the reference is gone, the circuit stays
    CHECK(a.referenceBrakeZoneCount() == 0);
    CHECK(a.mapState() == MapState::Complete);
    float delta, entry, ref;
    CHECK(!a.lastBrakeResult(delta, entry, ref));
    recordReference(d);
    a.reset();
    CHECK(a.referenceBrakeZoneCount() == 0 && a.brakeZoneCount() == 0);
    delete &a;
}

int main()
{
    testRecordsTheReferenceZones();
    testEarlierAndLaterThanTheReference();
    testDistanceToTheNextPoint();
    testSkippedPointIsPassed();
    testWhatIsNotABrakingZone();
    testPulsingIsOneZone();
    testRewindFindsTheNextPointAgain();
    testResets();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    printf("braking points: all checks passed\n");
    return 0;
}
