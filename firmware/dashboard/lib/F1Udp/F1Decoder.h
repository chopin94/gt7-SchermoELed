// Guard by name too: the LED strip reaches this file by two paths.
#ifndef F1UDP_F1DECODER_H
#define F1UDP_F1DECODER_H
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// F1 25 UDP telemetry, reduced to the player's car. The game (PC and
// consoles) sends it to port 20777 once enabled in Settings > Telemetry:
// UDP Telemetry On, UDP Broadcast Mode On (so the screen and the LED strip
// both receive it), UDP Format 2025. Formats 2024 (F1 24 and the "2024"
// choice of F1 25) and 2026 (the 2026 Season Pack: 24 cars, a few smaller
// fields) are read as well.
//
// The packets of one frame arrive separately: motion, lap data, status and
// telemetry. The decoder keeps the latest of each and completes a Car on
// every car telemetry packet, with the fields the GT7 code of the two
// firmwares already uses (GT7 wheel order: front left, front right, rear
// left, rear right). Plain C++11, so the same code runs on the ESP32 and in
// the tests on the PC (firmware/dashboard/tests/f1_decoder.cpp).
namespace F1Udp
{
static constexpr uint16_t PORT = 20777;
static constexpr size_t MAX_PACKET = 1500; // the largest is ~1460 bytes

struct Car
{
    // A 60 Hz clock in the style of the GT7 packet id: grows with the time
    // of arrival and never repeats, the lap analysis takes its time from it.
    int32_t clock60 = 0;
    float rpm = 0;
    float speed = 0;          // m/s
    int gear = 0;             // -1 reverse, 0 neutral, 1-8
    int suggestedGear = 0;    // 0: none
    float throttle = 0, brake = 0, clutch = 0; // 0-1
    float position[3] = {0, 0, 0}; // metres, y up as in GT7
    float velocity[3] = {0, 0, 0}; // m/s
    // Rev lights: the game's own shift light, 0-100 %.
    uint8_t revLightsPercent = 0;
    float maxRpm = 0;   // rev limiter
    float alertRpm = 0; // where the rev lights start (learned)
    float shiftRpm = 0; // where they are all on (learned)
    float fuel = NAN, fuelCapacity = NAN, fuelLaps = NAN; // kg, laps on the MFD
    float tyreTemp[4] = {NAN, NAN, NAN, NAN};  // surface, °C
    float brakeTemp[4] = {NAN, NAN, NAN, NAN}; // °C
    float engineTemp = NAN;                    // °C
    bool hasWheels = false;                    // motion ex packet seen
    float wheelSpeed[4] = {0, 0, 0, 0};        // m/s
    float suspension[4] = {NAN, NAN, NAN, NAN};
    int lap = 0, totalLaps = 0; // lap being driven, 1 on the first one
    int racePosition = 0, cars = 0;
    int32_t currentLapMs = -1, lastLapMs = -1, bestLapMs = -1;
    bool lapInvalid = false;
    bool inPit = false, pitLimiter = false, drs = false;
    bool paused = false;
    bool onTrack = false; // out of the garage, not paused
    int8_t fiaFlag = 0;   // 0 none, 1 green, 2 blue, 3 yellow, 4 red
    int teamId = -1;      // -1 until the participants packet arrives
    int trackId = -1;
    uint64_t session = 0;
};

class Decoder
{
public:
    // One UDP datagram. True when `car` holds a new sample of the player.
    bool feed(const uint8_t *data, size_t size, uint32_t nowMs, Car &car)
    {
        if (size < HEADER || !layout(get<uint16_t>(data, 0))) return false;
        const uint8_t id = data[6];
        const uint64_t session = get<uint64_t>(data, 7);
        const uint8_t player = data[27];
        if (session != state.session)
        {
            // Another session: lap times start over. A new car resets the
            // rev lights (participants packet).
            const int team = state.teamId, track = state.trackId;
            state = Car();
            state.session = session;
            state.teamId = team;
            state.trackId = track;
            ownBest = historyBest = -1;
            haveLapData = false;
            previousLap = 0;
            previousInvalid = false;
        }
        if (player >= cars) return false; // spectating
        switch (id)
        {
        case MOTION: motion(data, size, player); return false;
        case SESSION: sessionInfo(data, size); return false;
        case LAP_DATA: lapData(data, size, player); return false;
        case PARTICIPANTS: participants(data, size, player); return false;
        case CAR_STATUS: status(data, size, player); return false;
        case SESSION_HISTORY: history(data, size, player); return false;
        case MOTION_EX: motionEx(data, size); return false;
        case CAR_TELEMETRY:
            if (!telemetry(data, size, player)) return false;
            break;
        default: return false;
        }
        // The clock: arrival time at 60 Hz, at least one step per sample.
        const int32_t tick = static_cast<int32_t>(static_cast<uint64_t>(nowMs) * 3 / 50);
        state.clock60 = clockStarted && tick <= lastClock ? lastClock + 1 : tick;
        lastClock = state.clock60;
        clockStarted = true;
        state.bestLapMs = historyBest > 0 ? historyBest : ownBest;
        state.onTrack = !state.paused && driverStatus != 0 && haveLapData;
        car = state;
        return true;
    }

private:
    enum : uint8_t
    {
        MOTION = 0, SESSION = 1, LAP_DATA = 2, PARTICIPANTS = 4,
        CAR_TELEMETRY = 6, CAR_STATUS = 7, SESSION_HISTORY = 11, MOTION_EX = 13,
    };
    static constexpr size_t HEADER = 29;
    static constexpr size_t LAP_STRIDE = 57;
    static constexpr size_t HISTORY_LAP = 14;

    Car state;
    // Sizes of one car in each array, by packet format.
    size_t cars = 22, motionStride = 60, telemetryStride = 60, statusStride = 55;
    bool wideIds = false; // 2026: 16-bit driver, network and team ids
    int32_t lastClock = 0;
    bool clockStarted = false;
    uint8_t driverStatus = 0;
    bool haveLapData = false;
    bool gamePaused = false, networkPaused = false;
    int32_t ownBest = -1, historyBest = -1;
    int previousLap = 0;
    bool previousInvalid = false;

    template <typename T>
    static T get(const uint8_t *data, size_t offset)
    {
        T value;
        memcpy(&value, data + offset, sizeof(T));
        return value;
    }

    // Front left, front right, rear left, rear right from the F1 order
    // (rear left, rear right, front left, front right).
    static int wheel(int gt7Index) { return gt7Index < 2 ? gt7Index + 2 : gt7Index - 2; }

    bool layout(uint16_t format)
    {
        if (format == 2024 || format == 2025)
        {
            cars = 22; motionStride = 60; telemetryStride = 60; statusStride = 55;
            wideIds = false;
            return true;
        }
        if (format == 2026)
        {
            cars = 24; motionStride = 54; telemetryStride = 59; statusStride = 59;
            wideIds = true;
            return true;
        }
        return false;
    }

    void motion(const uint8_t *d, size_t size, uint8_t player)
    {
        if (size < HEADER + cars * motionStride) return;
        const size_t o = HEADER + player * motionStride;
        for (int i = 0; i < 3; ++i)
        {
            state.position[i] = get<float>(d, o + i * 4);
            state.velocity[i] = get<float>(d, o + 12 + i * 4);
        }
    }

    void sessionInfo(const uint8_t *d, size_t size)
    {
        if (size < HEADER + 15) return;
        const int sessionType = d[HEADER + 6];
        state.trackId = static_cast<int8_t>(d[HEADER + 7]);
        gamePaused = d[HEADER + 14] != 0;
        // Laps to go only in a race (15-17: race, race 2, race 3).
        state.totalLaps = sessionType >= 15 && sessionType <= 17 ? d[HEADER + 3] : 0;
        state.paused = gamePaused || networkPaused;
    }

    void lapData(const uint8_t *d, size_t size, uint8_t player)
    {
        if (size < HEADER + cars * LAP_STRIDE) return;
        const size_t o = HEADER + player * LAP_STRIDE;
        const uint32_t last = get<uint32_t>(d, o);
        const uint32_t current = get<uint32_t>(d, o + 4);
        state.lastLapMs = last > 0 ? static_cast<int32_t>(last) : -1;
        state.currentLapMs = static_cast<int32_t>(current);
        state.racePosition = d[o + 32];
        state.lap = d[o + 33];
        state.inPit = d[o + 34] != 0;
        state.lapInvalid = d[o + 37] != 0;
        driverStatus = d[o + 44];
        haveLapData = true;
        // Best valid lap of this session, until the session history says it.
        if (state.lap == previousLap + 1 && state.lastLapMs > 0 && !previousInvalid &&
            (ownBest < 0 || state.lastLapMs < ownBest))
            ownBest = state.lastLapMs;
        if (state.lap != previousLap) previousLap = state.lap;
        previousInvalid = state.lapInvalid;
    }

    void participants(const uint8_t *d, size_t size, uint8_t player)
    {
        if (size < HEADER + 1) return;
        const size_t stride = (size - HEADER - 1) / cars;
        if (stride < 8) return;
        state.cars = d[HEADER];
        const size_t o = HEADER + 1 + player * stride;
        const int team = wideIds ? get<uint16_t>(d, o + 5) : d[o + 3];
        if (team != state.teamId) forgetRevLights();
        state.teamId = team;
    }

    void status(const uint8_t *d, size_t size, uint8_t player)
    {
        if (size < HEADER + cars * statusStride) return;
        const size_t o = HEADER + player * statusStride;
        state.pitLimiter = d[o + 4] != 0;
        state.fuel = get<float>(d, o + 5);
        state.fuelCapacity = get<float>(d, o + 9);
        state.fuelLaps = get<float>(d, o + 13);
        const float maxRpm = get<uint16_t>(d, o + 17);
        if (maxRpm != state.maxRpm && state.maxRpm > 0) forgetRevLights();
        state.maxRpm = maxRpm;
        state.fiaFlag = static_cast<int8_t>(d[o + 28]);
        networkPaused = d[o + statusStride - 1] != 0;
        state.paused = gamePaused || networkPaused;
    }

    void history(const uint8_t *d, size_t size, uint8_t player)
    {
        if (size < HEADER + 7 || d[HEADER] != player) return;
        const int bestLap = d[HEADER + 3];
        const size_t o = HEADER + 7 + (bestLap - 1) * HISTORY_LAP;
        if (bestLap < 1 || size < o + HISTORY_LAP) { historyBest = -1; return; }
        const uint32_t ms = get<uint32_t>(d, o);
        historyBest = ms > 0 ? static_cast<int32_t>(ms) : -1;
    }

    void motionEx(const uint8_t *d, size_t size)
    {
        if (size < HEADER + 64) return;
        for (int i = 0; i < 4; ++i)
        {
            state.suspension[i] = get<float>(d, HEADER + wheel(i) * 4);
            state.wheelSpeed[i] = get<float>(d, HEADER + 48 + wheel(i) * 4);
        }
        state.hasWheels = true;
    }

    bool telemetry(const uint8_t *d, size_t size, uint8_t player)
    {
        if (size < HEADER + cars * telemetryStride + 3) return false;
        const size_t o = HEADER + player * telemetryStride;
        state.speed = get<uint16_t>(d, o) / 3.6f;
        state.throttle = clamp01(get<float>(d, o + 2));
        state.brake = clamp01(get<float>(d, o + 10));
        state.clutch = d[o + 14] / 100.0f;
        state.gear = static_cast<int8_t>(d[o + 15]);
        state.rpm = get<uint16_t>(d, o + 16);
        state.drs = d[o + 18] != 0;
        state.revLightsPercent = d[o + 19];
        const uint16_t revBits = get<uint16_t>(d, o + 20);
        for (int i = 0; i < 4; ++i)
        {
            state.brakeTemp[i] = get<uint16_t>(d, o + 22 + wheel(i) * 2);
            state.tyreTemp[i] = d[o + 30 + wheel(i)];
        }
        state.engineTemp = telemetryStride == 60 ? get<uint16_t>(d, o + 38) : d[o + 38];
        state.suggestedGear = static_cast<int8_t>(d[HEADER + cars * telemetryStride + 2]);
        learnRevLights(state.revLightsPercent >= 100 || revBits == 0x7FFF);
        return true;
    }

    // The rev lights grow with the rpm: the lowest rpm that lights the first
    // one is where they start, the lowest that lights them all is the shift
    // point. Until seen, a guess from the rev limiter.
    void learnRevLights(bool full)
    {
        if (state.gear >= 1 && state.rpm > 1000)
        {
            if (state.revLightsPercent > 0 && (learnedAlert <= 0 || state.rpm < learnedAlert)) learnedAlert = state.rpm;
            if (full && (learnedShift <= 0 || state.rpm < learnedShift)) learnedShift = state.rpm;
        }
        const float top = state.maxRpm > 1000 ? state.maxRpm : 13000.0f;
        state.shiftRpm = learnedShift > 0 ? learnedShift : top * 0.95f;
        state.alertRpm = learnedAlert > 0 && learnedAlert < state.shiftRpm ? learnedAlert : state.shiftRpm * 0.85f;
    }

    void forgetRevLights() { learnedAlert = learnedShift = 0; }

    static float clamp01(float v) { return isfinite(v) ? (v < 0 ? 0 : v > 1 ? 1 : v) : 0; }

    float learnedAlert = 0, learnedShift = 0;
};

// The GT7 encodings, for the code written for GT7 packets.
// Gear nibble: 0 reverse, 15 neutral; suggested gear 15 = none.
inline uint8_t gt7Gear(int gear) { return gear < 0 ? 0 : gear == 0 ? 15 : gear > 14 ? 14 : static_cast<uint8_t>(gear); }
inline uint8_t gt7SuggestedGear(int gear) { return gear >= 1 && gear <= 14 ? static_cast<uint8_t>(gear) : 15; }
// SimulatorFlags: car on track, paused, in gear, shift light blinking.
inline uint16_t gt7Flags(const Car &car)
{
    return (car.onTrack ? 1 << 0 : 0) | (car.paused ? 1 << 1 : 0) | (car.gear != 0 ? 1 << 3 : 0) |
           (car.revLightsPercent >= 100 ? 1 << 5 : 0);
}
// A car code per team, so another team starts the lap times over and the
// saved circuits are kept per team ("F1" + team).
inline int32_t gt7CarCode(const Car &car) { return car.teamId >= 0 ? 0x46310000 + car.teamId + 1 : 0; }
} // namespace F1Udp
#endif
