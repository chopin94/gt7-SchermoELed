#pragma once
#include <math.h>
#include <stdint.h>

// Grip analysis computed on the dashboard from the raw GT7 telemetry: how much
// each wheel slips against the ground (lock-ups under braking, wheelspin under
// power), tyre temperatures, suspension load and how much of the car's grip is
// being used. Plain C++11 without Arduino types and without dynamic memory, so
// the same code runs on the ESP32, in the unit tests (tests/grip_analysis.cpp)
// and in the screenshot renderer (tools/screenshots).
//
// Wheel order everywhere: front left, front right, rear left, rear right.
namespace GripAnalysis
{
static constexpr int WHEELS = 4;

// Slip ratio = (wheel speed - ground speed) / ground speed. Cornering alone
// moves it by a few percent; ABS and traction control keep it well inside
// these limits, so beyond them a wheel is really locked or spinning.
static constexpr float MIN_SPEED = 8.0f;      // m/s: below it the ratio is noise
static constexpr float LOCK_SLIP = -0.12f;
static constexpr float SPIN_SLIP = 0.10f;
static constexpr float LOCK_BRAKE = 0.10f;    // pedal needed for a lock-up
static constexpr float SPIN_THROTTLE = 0.15f; // pedal needed for wheelspin
static constexpr float DISPLAY_SLIP = 0.30f;  // slip at the end of the bars

static constexpr uint32_t HOLD_MS = 600;      // how long a lock-up stays on screen
static constexpr int ENTER_SAMPLES = 3;       // 50 ms at 60 Hz
static constexpr int EXIT_SAMPLES = 10;
static constexpr uint32_t COOLDOWN_MS = 400;  // ABS pulses count as one event
static constexpr float MIN_PEAK_G = 0.7f;
static constexpr float G_FILTER = 0.35f;
static constexpr float SUSPENSION_FLOOR = 0.0005f; // smallest movement that counts

enum class WheelState : uint8_t
{
    Unknown,  // standing still or no wheel data
    Grip,     // rolling normally
    Locking,  // slower than the ground under braking
    Spinning, // faster than the ground under power
};

// One telemetry update. Fields a game does not report keep their defaults.
struct Sample
{
    uint32_t timeMs = 0;          // sample clock
    float speed = 0;              // m/s, ground speed of the car
    bool driving = true;          // false while paused, loading or in the menus
    bool hasWheels = false;       // wheel speeds and radii are valid
    float wheelRps[WHEELS] = {0, 0, 0, 0};   // rad/s
    float tyreRadius[WHEELS] = {0, 0, 0, 0}; // m
    float throttle = 0;           // 0-1
    float brake = 0;              // 0-1
    float tyreTemp[WHEELS] = {NAN, NAN, NAN, NAN};   // degrees C
    float suspension[WHEELS] = {NAN, NAN, NAN, NAN}; // ride height per corner
    float lateralG = 0;           // + towards the right
    float longitudinalG = 0;      // + accelerating
    int lapCount = 0;             // lap counter of the game
};

class Analyzer
{
public:
    Analyzer() { reset(); }

    // Everything, including what was learnt about the car.
    void reset()
    {
        resetCar();
        lapKnown = false;
        lapCount = 0;
        lapLocks = lapSpins = lastLapLocks = lastLapSpins = 0;
        revision++;
    }

    // Another car: the learnt ranges start over.
    void resetCar()
    {
        for (int i = 0; i < WHEELS; ++i)
        {
            slipValue[i] = NAN;
            stateValue[i] = WheelState::Unknown;
            pending[i] = WheelState::Unknown;
            streak[i] = 0;
            activeState[i] = WheelState::Unknown;
            activeMs[i] = 0;
            temp[i] = NAN;
            suspRest[i] = NAN;
            suspPeak[i] = SUSPENSION_FLOOR;
            suspLoad[i] = 0;
        }
        suspVote = 0;
        wheelsSeen = false;
        eventState = WheelState::Unknown;
        eventCooldown = 0;
        peakG = combinedG = gLat = gLong = 0;
        hasPrevious = false;
        revision++;
    }

    void update(const Sample &s)
    {
        if (!s.driving)
        {
            // Paused or in the menus: nothing is measured.
            clearTransient();
            hasPrevious = false;
            return;
        }
        const int32_t dtMs = hasPrevious ? static_cast<int32_t>(s.timeMs - previousMs) : 0;
        if (dtMs < 0 || dtMs > 1000) clearTransient();
        previousMs = s.timeMs;
        hasPrevious = true;

        updateLap(s);
        updateSlip(s);
        updateEvents(s);
        updateTemperature(s);
        updateGrip(s);
        updateSuspension(s);
    }

    // ---- Wheels -------------------------------------------------------------
    // The game sends wheel data (False for Assetto Corsa and SimHub).
    bool wheelDataAvailable() const { return wheelsSeen; }
    // Filtered slip ratio, NAN when not measurable (slow or no wheel data).
    float slip(int wheel) const { return valid(wheel) ? slipValue[wheel] : NAN; }
    // The state, kept for a moment after it ends so a short lock-up is seen.
    // The moment is measured on the clock of the samples.
    WheelState state(int wheel) const
    {
        if (!valid(wheel)) return WheelState::Unknown;
        const WheelState now = stateValue[wheel];
        if (now == WheelState::Locking || now == WheelState::Spinning) return now;
        if (activeState[wheel] != WheelState::Unknown &&
            static_cast<int32_t>(previousMs - activeMs[wheel]) < static_cast<int32_t>(HOLD_MS))
            return activeState[wheel];
        return now;
    }
    // The state right now, without the hold of state().
    WheelState currentState(int wheel) const { return valid(wheel) ? stateValue[wheel] : WheelState::Unknown; }
    // Average of the two wheels of an axle (0 front, 1 rear), NAN if unknown.
    float axleSlip(int axle) const
    {
        const float a = slip(axle * 2), b = slip(axle * 2 + 1);
        if (!isfinite(a) || !isfinite(b)) return NAN;
        return 0.5f * (a + b);
    }

    // ---- Tyres and suspension -------------------------------------------------
    float temperature(int wheel) const { return valid(wheel) ? temp[wheel] : NAN; }
    // Load on the corner: 0 at rest, +1 fully loaded, -1 unloaded, relative to
    // the largest movement seen with this car.
    float suspensionLoad(int wheel) const { return valid(wheel) ? suspLoad[wheel] : 0.0f; }
    // True once hard braking has shown which way the suspension value moves.
    bool suspensionCalibrated() const { return fabsf(suspVote) > 0.6f; }

    // ---- Grip usage -------------------------------------------------------------
    // Combined lateral and longitudinal acceleration in g.
    float totalG() const { return combinedG; }
    // 0-1.2: the acceleration against the best seen with this car.
    float gripUsage() const
    {
        const float reference = maxf(peakG, MIN_PEAK_G);
        return minf(combinedG / reference, 1.2f);
    }
    float peakTotalG() const { return peakG; }

    // ---- Lap counters ----------------------------------------------------------------
    int lockEvents() const { return lapLocks; }
    int spinEvents() const { return lapSpins; }
    int lastLapLockEvents() const { return lastLapLocks; }
    int lastLapSpinEvents() const { return lastLapSpins; }
    // Changes whenever a counter changes: lets the themes skip redrawing.
    uint32_t counterRevision() const { return revision; }

private:
    float slipValue[WHEELS];
    WheelState stateValue[WHEELS];
    WheelState pending[WHEELS]; // state being confirmed
    int streak[WHEELS];         // samples in a row in `pending`
    WheelState activeState[WHEELS];
    uint32_t activeMs[WHEELS];

    float temp[WHEELS];
    float suspRest[WHEELS], suspPeak[WHEELS], suspLoad[WHEELS];
    float suspVote;

    WheelState eventState;
    uint32_t eventCooldown;

    float peakG, combinedG, gLat, gLong;

    bool hasPrevious = false;
    uint32_t previousMs = 0;
    bool wheelsSeen = false;

    bool lapKnown = false;
    int lapCount = 0;
    int lapLocks = 0, lapSpins = 0, lastLapLocks = 0, lastLapSpins = 0;
    uint32_t revision = 0;

    static bool valid(int wheel) { return wheel >= 0 && wheel < WHEELS; }
    static float maxf(float a, float b) { return a > b ? a : b; }
    static float minf(float a, float b) { return a < b ? a : b; }
    static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

    void clearTransient()
    {
        for (int i = 0; i < WHEELS; ++i)
        {
            slipValue[i] = NAN;
            stateValue[i] = WheelState::Unknown;
            pending[i] = WheelState::Unknown;
            streak[i] = 0;
            activeState[i] = WheelState::Unknown;
        }
        eventState = WheelState::Unknown;
    }

    void updateLap(const Sample &s)
    {
        if (!lapKnown) { lapKnown = true; lapCount = s.lapCount; return; }
        if (s.lapCount == lapCount) return;
        lapCount = s.lapCount;
        lastLapLocks = lapLocks;
        lastLapSpins = lapSpins;
        lapLocks = lapSpins = 0;
        revision++;
    }

    void updateSlip(const Sample &s)
    {
        wheelsSeen = s.hasWheels;
        for (int i = 0; i < WHEELS; ++i)
        {
            const bool ok = s.hasWheels && s.speed >= MIN_SPEED && isfinite(s.wheelRps[i]) &&
                isfinite(s.tyreRadius[i]) && s.tyreRadius[i] > 0.05f && s.tyreRadius[i] < 1.5f;
            if (!ok)
            {
                slipValue[i] = NAN;
                stateValue[i] = WheelState::Unknown;
                pending[i] = WheelState::Unknown;
                streak[i] = 0;
                continue;
            }
            const float wheelSpeed = fabsf(s.wheelRps[i]) * s.tyreRadius[i];
            const float raw = clampf((wheelSpeed - s.speed) / s.speed, -1.0f, 1.0f);
            slipValue[i] = isfinite(slipValue[i]) ? slipValue[i] + 0.5f * (raw - slipValue[i]) : raw;

            WheelState next = WheelState::Grip;
            if (s.brake >= LOCK_BRAKE && slipValue[i] <= LOCK_SLIP) next = WheelState::Locking;
            else if (s.throttle >= SPIN_THROTTLE && slipValue[i] >= SPIN_SLIP) next = WheelState::Spinning;

            if (stateValue[i] == WheelState::Unknown)
            {
                // First measurement after standing still.
                stateValue[i] = pending[i] = WheelState::Grip;
                streak[i] = 0;
            }
            // A state needs a few samples in a row, a single bump is noise.
            if (next == pending[i]) { if (streak[i] < 1000) streak[i]++; }
            else { pending[i] = next; streak[i] = 1; }
            if (pending[i] != stateValue[i] &&
                streak[i] >= (pending[i] == WheelState::Grip ? EXIT_SAMPLES : ENTER_SAMPLES))
                stateValue[i] = pending[i];

            if (stateValue[i] == WheelState::Locking || stateValue[i] == WheelState::Spinning)
            {
                activeState[i] = stateValue[i];
                activeMs[i] = s.timeMs;
            }
        }
    }

    // One event per lock-up or wheelspin, whatever the number of wheels.
    void updateEvents(const Sample &s)
    {
        WheelState worst = WheelState::Grip;
        bool any = false;
        for (int i = 0; i < WHEELS; ++i)
        {
            if (stateValue[i] == WheelState::Unknown) continue;
            any = true;
            if (stateValue[i] == WheelState::Locking) worst = WheelState::Locking;
            else if (stateValue[i] == WheelState::Spinning && worst != WheelState::Locking)
                worst = WheelState::Spinning;
        }
        if (!any) { eventState = WheelState::Unknown; return; }
        if (worst == eventState) return;
        const bool wasActive = eventState == WheelState::Locking || eventState == WheelState::Spinning;
        const bool nowActive = worst == WheelState::Locking || worst == WheelState::Spinning;
        // ABS keeps pulsing a wheel: that is one lock-up, not twenty.
        if (nowActive && static_cast<int32_t>(s.timeMs - eventCooldown) >= 0)
        {
            if (worst == WheelState::Locking) lapLocks++; else lapSpins++;
            revision++;
        }
        if (wasActive && !nowActive) eventCooldown = s.timeMs + COOLDOWN_MS;
        eventState = worst;
    }

    void updateTemperature(const Sample &s)
    {
        for (int i = 0; i < WHEELS; ++i)
            temp[i] = (isfinite(s.tyreTemp[i]) && s.tyreTemp[i] > -40.0f && s.tyreTemp[i] < 300.0f)
                ? s.tyreTemp[i] : NAN;
    }

    void updateGrip(const Sample &s)
    {
        gLat += G_FILTER * (s.lateralG - gLat);
        gLong += G_FILTER * (s.longitudinalG - gLong);
        combinedG = sqrtf(gLat * gLat + gLong * gLong);
        if (s.speed >= 10.0f && combinedG < 6.0f)
        {
            if (combinedG > peakG) peakG = combinedG;
            // The peak fades slowly so one wall hit does not set it for good.
            else peakG -= peakG * 0.00004f;
        }
    }

    // Suspension: each corner is compared with its own value at rest and the
    // result is scaled by the largest movement seen. The direction is learnt:
    // under hard braking the front is the loaded axle.
    void updateSuspension(const Sample &s)
    {
        float dev[WHEELS];
        for (int i = 0; i < WHEELS; ++i)
        {
            if (!isfinite(s.suspension[i])) return;
        }
        const bool steady = fabsf(gLat) < 0.25f && fabsf(gLong) < 0.15f;
        for (int i = 0; i < WHEELS; ++i)
        {
            if (!isfinite(suspRest[i])) suspRest[i] = s.suspension[i];
            // The rest value follows only in quiet moments: ride height changes
            // with fuel and downforce, not with a corner or a braking zone.
            if (steady) suspRest[i] += 0.01f * (s.suspension[i] - suspRest[i]);
            dev[i] = s.suspension[i] - suspRest[i];
            const float magnitude = fabsf(dev[i]);
            if (magnitude > suspPeak[i]) suspPeak[i] = magnitude;
            else suspPeak[i] = maxf(SUSPENSION_FLOOR, suspPeak[i] * 0.99998f);
        }
        if (s.longitudinalG < -0.6f && s.speed > 15.0f)
        {
            const float front = 0.5f * (dev[0] + dev[1]), rear = 0.5f * (dev[2] + dev[3]);
            // +1: the value grows when a corner is loaded.
            suspVote += 0.02f * ((front > rear ? 1.0f : -1.0f) - suspVote);
        }
        // Until braking has shown the direction, a lower ride height means load.
        const float sign = suspVote > 0.2f ? 1.0f : -1.0f;
        for (int i = 0; i < WHEELS; ++i)
        {
            const float load = clampf(sign * dev[i] / suspPeak[i], -1.0f, 1.0f);
            suspLoad[i] += 0.3f * (load - suspLoad[i]);
        }
    }
};
} // namespace GripAnalysis
