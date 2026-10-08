// Tests of src/GripAnalysis.h:
//   g++ -std=c++11 -Wall -Wextra -pedantic tests/grip_analysis.cpp -o /tmp/grip-tests && /tmp/grip-tests
#include "../src/GripAnalysis.h"
#include <stdio.h>
#include <stdlib.h>

using namespace GripAnalysis;

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

static const float RADIUS = 0.33f;

// Drives samples at 60 Hz. `ratio[i]` is the wheel speed over the ground speed.
struct Rig
{
    Analyzer analyzer;
    uint32_t packet = 100000;
    int lap = 0;
    float speed = 50.0f;
    float throttle = 0, brake = 0;
    float ratio[WHEELS] = {1, 1, 1, 1};
    float lateral = 0, longitudinal = 0;
    float suspension[WHEELS] = {NAN, NAN, NAN, NAN};

    uint32_t now() const { return static_cast<uint32_t>(static_cast<int64_t>(packet) * 50 / 3); }

    void step()
    {
        Sample s;
        s.timeMs = now();
        s.speed = speed;
        s.hasWheels = true;
        for (int i = 0; i < WHEELS; ++i)
        {
            s.tyreRadius[i] = RADIUS;
            s.wheelRps[i] = ratio[i] * speed / RADIUS;
            s.tyreTemp[i] = 80.0f + i;
            s.suspension[i] = suspension[i];
        }
        s.throttle = throttle;
        s.brake = brake;
        s.lateralG = lateral;
        s.longitudinalG = longitudinal;
        s.lapCount = lap;
        analyzer.update(s);
        packet++;
    }
    void run(int samples) { for (int i = 0; i < samples; ++i) step(); }
    void runMs(int ms) { run(ms * 60 / 1000); }
    void allWheels(float r) { for (int i = 0; i < WHEELS; ++i) ratio[i] = r; }
};

static void testRolling()
{
    Rig r;
    r.throttle = 0.3f;
    r.run(120);
    for (int i = 0; i < WHEELS; ++i)
    {
        CHECK_NEAR(r.analyzer.slip(i), 0.0, 0.001);
        CHECK(r.analyzer.state(i) == WheelState::Grip);
        CHECK_NEAR(r.analyzer.temperature(i), 80.0 + i, 0.001);
    }
    CHECK(r.analyzer.lockEvents() == 0 && r.analyzer.spinEvents() == 0);
    CHECK_NEAR(r.analyzer.axleSlip(0), 0.0, 0.001);
}

static void testCorneringIsNotSlip()
{
    Rig r;
    r.throttle = 0.5f;
    // Outer wheels 2% faster, inner 2% slower, 35 m/s round a corner.
    r.speed = 35.0f;
    r.ratio[0] = 0.98f; r.ratio[1] = 1.02f; r.ratio[2] = 0.985f; r.ratio[3] = 1.015f;
    r.run(180);
    for (int i = 0; i < WHEELS; ++i) CHECK(r.analyzer.state(i) == WheelState::Grip);
    CHECK(r.analyzer.lockEvents() == 0 && r.analyzer.spinEvents() == 0);
}

static void testLockUp()
{
    Rig r;
    r.run(60);
    r.brake = 0.9f;
    r.ratio[0] = 0.70f; // front left locks
    r.run(2);
    // Two samples are not enough: a bump is not a lock-up.
    CHECK(r.analyzer.state(0) == WheelState::Grip);
    r.run(10);
    CHECK(r.analyzer.state(0) == WheelState::Locking);
    CHECK(r.analyzer.state(1) == WheelState::Grip);
    CHECK(r.analyzer.lockEvents() == 1);
    CHECK(r.analyzer.slip(0) < -0.2f);
    CHECK(r.analyzer.axleSlip(0) < -0.1f);

    // The wheel recovers: the state ends after a moment but stays visible.
    r.ratio[0] = 1.0f;
    r.runMs(250);
    CHECK(r.analyzer.currentState(0) == WheelState::Grip);
    CHECK(r.analyzer.state(0) == WheelState::Locking); // held for the display
    r.runMs(800);
    CHECK(r.analyzer.state(0) == WheelState::Grip);
    CHECK(r.analyzer.lockEvents() == 1);
}

static void testAbsPulsesAreOneEvent()
{
    Rig r;
    r.run(60);
    r.brake = 1.0f;
    // ABS releases and clamps a wheel every 6 samples for 0.7 s.
    for (int cycle = 0; cycle < 7; ++cycle)
    {
        r.ratio[1] = 0.75f; r.run(6);
        r.ratio[1] = 1.0f; r.run(6);
    }
    CHECK(r.analyzer.lockEvents() == 1);
    // Much later, a second lock-up is a second event.
    r.brake = 0; r.ratio[1] = 1.0f;
    r.runMs(1500);
    r.brake = 1.0f; r.ratio[1] = 0.6f;
    r.run(20);
    CHECK(r.analyzer.lockEvents() == 2);
}

static void testWheelspin()
{
    Rig r;
    r.speed = 25.0f;
    r.throttle = 1.0f;
    r.run(30);
    r.ratio[3] = 1.30f;
    r.run(15);
    CHECK(r.analyzer.state(3) == WheelState::Spinning);
    CHECK(r.analyzer.state(2) == WheelState::Grip);
    CHECK(r.analyzer.spinEvents() == 1);
    CHECK(r.analyzer.axleSlip(1) > 0.1f);
    CHECK(r.analyzer.lockEvents() == 0);
}

static void testNeedsThePedal()
{
    Rig r;
    r.run(30);
    // Slower wheel without the brake (engine braking, a spin) is not a lock-up.
    r.ratio[0] = 0.8f;
    r.run(60);
    CHECK(r.analyzer.state(0) == WheelState::Grip);
    CHECK(r.analyzer.lockEvents() == 0);
    // Faster wheel without the throttle is not wheelspin.
    r.ratio[0] = 1.0f; r.ratio[3] = 1.3f;
    r.run(60);
    CHECK(r.analyzer.state(3) == WheelState::Grip);
    CHECK(r.analyzer.spinEvents() == 0);
}

static void testSlowSpeed()
{
    Rig r;
    r.speed = 4.0f; // below the minimum speed
    r.brake = 1.0f;
    r.ratio[0] = 0.0f;
    r.run(60);
    CHECK(r.analyzer.wheelDataAvailable()); // slow, but the game does send wheels
    CHECK(!isfinite(r.analyzer.slip(0)));
    CHECK(r.analyzer.state(0) == WheelState::Unknown);
    CHECK(r.analyzer.lockEvents() == 0);
    // Speeding up again starts clean.
    r.speed = 40.0f; r.brake = 0; r.ratio[0] = 1.0f;
    r.run(30);
    CHECK(r.analyzer.state(0) == WheelState::Grip);
}

static void testNoWheelData()
{
    Analyzer a;
    Sample s;
    s.hasWheels = false;
    s.speed = 50.0f;
    for (int i = 0; i < 60; ++i) { s.timeMs = i * 17; a.update(s); }
    CHECK(!a.wheelDataAvailable());
    CHECK(!isfinite(a.slip(0)));
    CHECK(a.state(2) == WheelState::Unknown);
    CHECK(!isfinite(a.axleSlip(1)));
    CHECK(!isfinite(a.temperature(0)));
    // Out-of-range wheel index.
    CHECK(!isfinite(a.slip(7)));
    CHECK(a.state(-1) == WheelState::Unknown);
}

static void testLaps()
{
    Rig r;
    r.run(30);
    r.brake = 1.0f; r.ratio[0] = 0.6f; r.run(20);
    r.brake = 0; r.ratio[0] = 1.0f; r.runMs(1200);
    r.throttle = 1.0f; r.ratio[2] = 1.4f; r.run(20);
    r.ratio[2] = 1.0f; r.runMs(1200);
    CHECK(r.analyzer.lockEvents() == 1 && r.analyzer.spinEvents() == 1);
    const uint32_t before = r.analyzer.counterRevision();
    r.lap = 1;
    r.run(2);
    CHECK(r.analyzer.counterRevision() != before);
    CHECK(r.analyzer.lockEvents() == 0 && r.analyzer.spinEvents() == 0);
    CHECK(r.analyzer.lastLapLockEvents() == 1 && r.analyzer.lastLapSpinEvents() == 1);
    r.lap = 2;
    r.run(2);
    CHECK(r.analyzer.lastLapLockEvents() == 0);
}

static void testGripUsage()
{
    Rig r;
    r.lateral = 1.2f; r.run(120);
    CHECK_NEAR(r.analyzer.totalG(), 1.2, 0.02);
    CHECK_NEAR(r.analyzer.gripUsage(), 1.0, 0.03);
    r.lateral = 0.6f; r.run(120);
    CHECK_NEAR(r.analyzer.gripUsage(), 0.5, 0.05);
    r.lateral = 0; r.longitudinal = -0.9f; r.run(120);
    CHECK(r.analyzer.gripUsage() < 0.85f);
    // A tiny car peak never reads as 100% at 0.2 g.
    Rig kart;
    kart.lateral = 0.2f; kart.run(120);
    CHECK(kart.analyzer.gripUsage() < 0.4f);
}

// Suspension heights: `loadedLower` says whether a loaded corner has a lower value.
static void driveSuspension(Rig &r, bool loadedLower)
{
    const float rest = 0.12f, travel = 0.02f;
    const float sign = loadedLower ? -1.0f : 1.0f;
    // Straight, steady: learn the rest value.
    r.speed = 40.0f; r.lateral = 0; r.longitudinal = 0;
    for (int i = 0; i < WHEELS; ++i) r.suspension[i] = rest;
    r.run(240);
    // Hard braking: the front axle takes the load, the rear loses it.
    r.longitudinal = -1.1f;
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        r.suspension[0] = r.suspension[1] = rest + sign * travel;
        r.suspension[2] = r.suspension[3] = rest - sign * travel * 0.5f;
        r.run(150);
        r.longitudinal = 0;
        for (int i = 0; i < WHEELS; ++i) r.suspension[i] = rest;
        r.run(240);
        r.longitudinal = -1.1f;
    }
    r.suspension[0] = r.suspension[1] = rest + sign * travel;
    r.suspension[2] = r.suspension[3] = rest - sign * travel * 0.5f;
    r.run(60);
}

static void testSuspensionLowerIsLoaded()
{
    Rig r;
    driveSuspension(r, true);
    CHECK(r.analyzer.suspensionLoad(0) > 0.5f);
    CHECK(r.analyzer.suspensionLoad(1) > 0.5f);
    CHECK(r.analyzer.suspensionLoad(2) < 0.0f);
}

static void testSuspensionLearnsDirection()
{
    // The values grow when a corner is loaded: braking teaches the direction.
    Rig r;
    driveSuspension(r, false);
    CHECK(r.analyzer.suspensionCalibrated());
    CHECK(r.analyzer.suspensionLoad(0) > 0.5f);
    CHECK(r.analyzer.suspensionLoad(3) < 0.0f);
}

static void testPauseAndReset()
{
    Rig r;
    r.brake = 1.0f; r.ratio[0] = 0.6f;
    r.run(30);
    CHECK(r.analyzer.lockEvents() == 1);
    Sample paused;
    paused.driving = false;
    paused.timeMs = r.now();
    r.analyzer.update(paused);
    // Paused: nothing is measured.
    CHECK(r.analyzer.currentState(0) == WheelState::Unknown);
    CHECK(!isfinite(r.analyzer.slip(0)));
    r.analyzer.resetCar();
    CHECK(r.analyzer.lockEvents() == 1); // lap counters survive a car change
    r.analyzer.reset();
    CHECK(r.analyzer.lockEvents() == 0);
    CHECK(!isfinite(r.analyzer.slip(0)));
}

static void testClockJump()
{
    Rig r;
    r.run(30);
    r.brake = 1.0f; r.ratio[1] = 0.5f;
    r.run(30);
    CHECK(r.analyzer.lockEvents() == 1);
    // The clock goes back (restart): no crash, the state is measured again.
    r.packet = 5000;
    r.run(30);
    CHECK(r.analyzer.state(1) == WheelState::Locking);
}

int main()
{
    testRolling();
    testCorneringIsNotSlip();
    testLockUp();
    testAbsPulsesAreOneEvent();
    testWheelspin();
    testNeedsThePedal();
    testSlowSpeed();
    testNoWheelData();
    testLaps();
    testGripUsage();
    testSuspensionLowerIsLoaded();
    testSuspensionLearnsDirection();
    testPauseAndReset();
    testClockJump();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    printf("grip analysis: all checks passed\n");
    return 0;
}
