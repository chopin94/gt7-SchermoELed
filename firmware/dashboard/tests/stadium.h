// A simulated driver on a stadium circuit for the braking and storage tests.
//
// 500 m straight along x at z = 0, right semicircle of radius 100 m, straight
// back at z = 200, left semicircle. The line is at (0, 0). The car leaves each
// corner at 25 m/s, accelerates to 60 m/s and brakes for the next one: the
// braking point can be moved earlier or later by a known distance, so the
// analysis can be checked against it.
#pragma once
#include "../src/LapAnalysis.h"
#include <math.h>

namespace stadium
{
static const float STRAIGHT = 500.0f, RADIUS = 100.0f;
static const float PI_F = 3.14159265f;
static const float TURN = PI_F * RADIUS;
static const float LENGTH = 2 * STRAIGHT + 2 * TURN;
static const float VMAX = 60.0f, VCORNER = 25.0f, ACCEL = 6.0f, DECEL = 12.0f;
// Metres after the corner exit where the nominal braking starts: the speed
// reached after the acceleration phase: braking from 60 to 25 m/s takes 124 m,
// which ends 10 m before the corner.
static const float NOMINAL_ONSET = 366.0f;

static inline void pointAt(float s, float &x, float &z, float &hx, float &hz)
{
    s = fmodf(s, LENGTH);
    if (s < 0) s += LENGTH;
    if (s < STRAIGHT) { x = s; z = 0; hx = 1; hz = 0; return; }
    s -= STRAIGHT;
    if (s < TURN)
    {
        const float a = s / RADIUS;
        x = STRAIGHT + RADIUS * sinf(a); z = RADIUS - RADIUS * cosf(a);
        hx = cosf(a); hz = sinf(a);
        return;
    }
    s -= TURN;
    if (s < STRAIGHT) { x = STRAIGHT - s; z = 2 * RADIUS; hx = -1; hz = 0; return; }
    s -= STRAIGHT;
    const float a = s / RADIUS;
    x = -RADIUS * sinf(a); z = RADIUS + RADIUS * cosf(a);
    hx = -cosf(a); hz = -sinf(a);
}

static inline float cruise(float metres)
{
    const float v = sqrtf(VCORNER * VCORNER + 2 * ACCEL * metres);
    return v < VMAX ? v : VMAX;
}

// Speed at distance s along the lap when the braking is `earlier` metres
// before the nominal point (negative: later).
static inline float profile(float s, float earlier)
{
    s = fmodf(s, LENGTH);
    if (s < 0) s += LENGTH;
    float t;
    if (s < STRAIGHT) t = s;
    else if (s < STRAIGHT + TURN) return VCORNER;
    else if (s < 2 * STRAIGHT + TURN) t = s - (STRAIGHT + TURN);
    else return VCORNER;
    const float onset = NOMINAL_ONSET - earlier;
    if (t < onset) return cruise(t);
    const float vOn = cruise(onset);
    const float v2 = vOn * vOn - 2 * DECEL * (t - onset);
    return v2 > VCORNER * VCORNER ? sqrtf(v2) : VCORNER;
}

struct Driver
{
    LapAnalysis::Analyzer &analyzer;
    bool fractionMode = false;
    float s = 0;
    double time = 0;
    int lapCount = 0;
    double lapStart = 0;
    int32_t lastLapMs = -1;
    bool lapStarted = false;
    float earlier = 0;        // braking point shift, metres
    float pace = 1.0f;        // speed factor (slower laps)
    float offsetX = 0, offsetZ = 0;
    bool jumpOnLine = false;

    explicit Driver(LapAnalysis::Analyzer &a) : analyzer(a) {}

    float speedAt(float at) const { return profile(at, earlier) * pace; }

    LapAnalysis::Sample sample(float speed, float throttle, float brake) const
    {
        LapAnalysis::Sample out;
        out.timeMs = static_cast<uint32_t>(llround(time * 1000.0));
        out.speed = speed;
        float x, z, hx, hz;
        pointAt(s, x, z, hx, hz);
        out.hasPosition = true;
        out.x = x + offsetX; out.z = z + offsetZ;
        out.hasVelocity = !fractionMode;
        out.vx = hx * speed; out.vz = hz * speed;
        out.lapCount = lapCount;
        out.totalLaps = 0;
        out.lapTimeMs = lapStarted ? static_cast<int32_t>(llround((time - lapStart) * 1000.0)) : 0;
        out.lastLapMs = lastLapMs;
        if (fractionMode) out.lapFraction = fmodf(s, LENGTH) / LENGTH;
        out.throttle = throttle;
        out.brake = brake;
        return out;
    }

    // One packet at 60 Hz.
    void step()
    {
        const double dt = 1.0 / 60.0;
        const float v = speedAt(s);
        // Deceleration and acceleration from the profile itself.
        const float ahead = speedAt(s + 0.5f), behind = speedAt(s - 0.5f);
        const float accel = v * (ahead - behind); // dv/dt = v dv/ds, 1 m apart
        const float brake = accel < -3.0f ? 0.85f : 0.0f;
        const float throttle = accel > 1.0f ? 0.7f : accel < -3.0f ? 0.0f : 0.25f;
        const float before = s;
        s += v * dt;
        time += dt;
        if (floorf(before / LENGTH) != floorf(s / LENGTH))
        {
            const double over = (s - floorf(s / LENGTH) * LENGTH) / v;
            const double crossing = time - over;
            if (lapStarted) lastLapMs = static_cast<int32_t>(llround((crossing - lapStart) * 1000.0));
            lapStart = crossing;
            lapStarted = true;
            lapCount++;
        }
        analyzer.update(sample(v, throttle, brake));
    }

    void drive(float distance)
    {
        const float end = s + distance;
        while (s < end) step();
    }
    // Drives until the lap counter reaches `lap`.
    void driveToLap(int lap)
    {
        while (lapCount < lap) step();
    }
    // Drives until the car next passes the point `at` metres along the lap.
    void driveTo(float at)
    {
        float target = floorf(s / LENGTH) * LENGTH + at;
        if (s >= target) target += LENGTH;
        while (s < target) step();
    }
};
} // namespace stadium
