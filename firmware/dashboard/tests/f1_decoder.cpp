// Tests of lib/F1Udp/F1Decoder.h with packets built byte by byte from the
// F1 25 specification (formats 2025 and 2026):
//   g++ -std=c++11 -Wall -Wextra -pedantic tests/f1_decoder.cpp -o /tmp/f1-tests && /tmp/f1-tests
#include "../lib/F1Udp/F1Decoder.h"
#include <stdio.h>
#include <vector>

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

struct Format
{
    uint16_t year;
    size_t cars, motion, telemetry, status, participant;
};
static const Format F2025 = {2025, 22, 60, 60, 55, 57};
static const Format F2026 = {2026, 24, 54, 59, 59, 61};

static const uint8_t PLAYER = 3;
static const uint64_t SESSION = 0x1122334455667788ULL;

struct Builder
{
    std::vector<uint8_t> b;
    template <typename T>
    void put(size_t offset, T value)
    {
        if (b.size() < offset + sizeof(T)) b.resize(offset + sizeof(T));
        memcpy(&b[offset], &value, sizeof(T));
    }
};

static Builder header(const Format &f, uint8_t id, size_t size, uint64_t session = SESSION)
{
    Builder p;
    p.b.assign(size, 0);
    p.put<uint16_t>(0, f.year);
    p.b[2] = 25;
    p.b[6] = id;
    p.put<uint64_t>(7, session);
    p.b[27] = PLAYER;
    p.b[28] = 255;
    return p;
}

static Builder motion(const Format &f, float x, float y, float z, float vx, float vz)
{
    Builder p = header(f, 0, 29 + f.cars * f.motion);
    const size_t o = 29 + PLAYER * f.motion;
    p.put<float>(o, x);
    p.put<float>(o + 4, y);
    p.put<float>(o + 8, z);
    p.put<float>(o + 12, vx);
    p.put<float>(o + 20, vz);
    return p;
}

static Builder session(const Format &f, uint8_t type, uint8_t laps, bool paused)
{
    Builder p = header(f, 1, 753);
    p.b[29 + 3] = laps;
    p.put<uint16_t>(29 + 4, 5412);
    p.b[29 + 6] = type;
    p.b[29 + 7] = 10; // Spa
    p.b[29 + 14] = paused ? 1 : 0;
    return p;
}

static Builder lapData(const Format &f, uint32_t last, uint32_t current, uint8_t lap, bool invalid,
                       uint8_t driverStatus = 4)
{
    Builder p = header(f, 2, 29 + f.cars * 57 + 2);
    const size_t o = 29 + PLAYER * 57;
    p.put<uint32_t>(o, last);
    p.put<uint32_t>(o + 4, current);
    p.b[o + 32] = 7;  // position
    p.b[o + 33] = lap;
    p.b[o + 37] = invalid ? 1 : 0;
    p.b[o + 44] = driverStatus;
    return p;
}

static Builder participants(const Format &f, int team)
{
    Builder p = header(f, 4, 29 + 1 + f.cars * f.participant);
    p.b[29] = 20;
    const size_t o = 30 + PLAYER * f.participant;
    if (f.year == 2026) p.put<uint16_t>(o + 5, static_cast<uint16_t>(team));
    else p.b[o + 3] = static_cast<uint8_t>(team);
    return p;
}

static Builder status(const Format &f, uint16_t maxRpm)
{
    Builder p = header(f, 7, 29 + f.cars * f.status);
    const size_t o = 29 + PLAYER * f.status;
    p.put<float>(o + 5, 55.0f);
    p.put<float>(o + 9, 110.0f);
    p.put<float>(o + 13, 12.5f);
    p.put<uint16_t>(o + 17, maxRpm);
    p.b[o + 28] = 3; // yellow flag
    return p;
}

static Builder telemetry(const Format &f, uint16_t kmh, int gear, uint16_t rpm, uint8_t revPercent,
                         float throttle = 1.0f, float brake = 0.0f)
{
    Builder p = header(f, 6, 29 + f.cars * f.telemetry + 3);
    const size_t o = 29 + PLAYER * f.telemetry;
    p.put<uint16_t>(o, kmh);
    p.put<float>(o + 2, throttle);
    p.put<float>(o + 10, brake);
    p.b[o + 14] = 40;
    p.b[o + 15] = static_cast<uint8_t>(static_cast<int8_t>(gear));
    p.put<uint16_t>(o + 16, rpm);
    p.b[o + 19] = revPercent;
    // Tyres in the F1 order: rear left, rear right, front left, front right
    const uint8_t tyres[4] = {81, 82, 91, 92};
    for (int i = 0; i < 4; ++i) p.b[o + 30 + i] = tyres[i];
    if (f.telemetry == 60) p.put<uint16_t>(o + 38, 105);
    else p.b[o + 38] = 105;
    p.b[29 + f.cars * f.telemetry + 2] = static_cast<uint8_t>(static_cast<int8_t>(gear + 1));
    return p;
}

static Builder motionEx(const Format &f)
{
    Builder p = header(f, 13, 273);
    const float susp[4] = {1, 2, 3, 4};      // RL RR FL FR
    const float speed[4] = {50, 51, 52, 53}; // RL RR FL FR
    for (int i = 0; i < 4; ++i)
    {
        p.put<float>(29 + i * 4, susp[i]);
        p.put<float>(29 + 48 + i * 4, speed[i]);
    }
    return p;
}

static Builder history(const Format &f, uint8_t car, uint8_t bestLap, uint32_t bestMs)
{
    Builder p = header(f, 11, 1460);
    p.b[29] = car;
    p.b[30] = 5;
    p.b[32] = bestLap;
    p.put<uint32_t>(29 + 7 + (bestLap - 1) * 14, bestMs);
    return p;
}

static bool feed(F1Udp::Decoder &d, const Builder &p, uint32_t now, F1Udp::Car &car)
{
    return d.feed(p.b.data(), p.b.size(), now, car);
}

static void testFormat(const Format &f)
{
    printf("format %u\n", f.year);
    F1Udp::Decoder d;
    F1Udp::Car car;
    // Only the car telemetry completes a sample.
    CHECK(!feed(d, motion(f, 100.5f, 3.0f, -250.25f, 70.0f, -10.0f), 1000, car));
    CHECK(!feed(d, session(f, 15, 44, false), 1000, car));
    CHECK(!feed(d, lapData(f, 0, 12345, 1, false), 1000, car));
    CHECK(!feed(d, participants(f, 8), 1000, car));
    CHECK(!feed(d, status(f, 13000), 1000, car));
    CHECK(!feed(d, motionEx(f), 1000, car));
    CHECK(feed(d, telemetry(f, 288, 7, 11000, 40, 0.75f, 0.25f), 1000, car));

    CHECK_NEAR(car.speed, 80.0, 0.01);
    CHECK(car.gear == 7);
    CHECK(car.suggestedGear == 8);
    CHECK_NEAR(car.rpm, 11000, 0);
    CHECK_NEAR(car.throttle, 0.75, 1e-6);
    CHECK_NEAR(car.brake, 0.25, 1e-6);
    CHECK_NEAR(car.clutch, 0.40, 1e-6);
    CHECK_NEAR(car.position[0], 100.5, 1e-4);
    CHECK_NEAR(car.position[1], 3.0, 1e-4);
    CHECK_NEAR(car.position[2], -250.25, 1e-4);
    CHECK_NEAR(car.velocity[0], 70.0, 1e-4);
    CHECK_NEAR(car.velocity[2], -10.0, 1e-4);
    // GT7 wheel order: front left, front right, rear left, rear right.
    CHECK_NEAR(car.tyreTemp[0], 91, 0);
    CHECK_NEAR(car.tyreTemp[1], 92, 0);
    CHECK_NEAR(car.tyreTemp[2], 81, 0);
    CHECK_NEAR(car.tyreTemp[3], 82, 0);
    CHECK(car.hasWheels);
    CHECK_NEAR(car.wheelSpeed[0], 52, 0);
    CHECK_NEAR(car.wheelSpeed[3], 51, 0);
    CHECK_NEAR(car.suspension[0], 3, 0);
    CHECK_NEAR(car.suspension[2], 1, 0);
    CHECK_NEAR(car.engineTemp, 105, 0);
    CHECK_NEAR(car.fuel, 55, 0);
    CHECK_NEAR(car.fuelCapacity, 110, 0);
    CHECK_NEAR(car.fuelLaps, 12.5, 1e-6);
    CHECK_NEAR(car.maxRpm, 13000, 0);
    CHECK(car.fiaFlag == 3);
    CHECK(car.lap == 1);
    CHECK(car.totalLaps == 44);
    CHECK(car.racePosition == 7);
    CHECK(car.cars == 20);
    CHECK(car.currentLapMs == 12345);
    CHECK(car.lastLapMs == -1);
    CHECK(car.bestLapMs == -1);
    CHECK(car.teamId == 8);
    CHECK(car.trackId == 10);
    CHECK(car.onTrack);
    CHECK(!car.paused);
    CHECK(F1Udp::gt7CarCode(car) == 0x46310009);
    CHECK(F1Udp::gt7Gear(car.gear) == 7);
    CHECK(F1Udp::gt7Gear(0) == 15);
    CHECK(F1Udp::gt7Gear(-1) == 0);
    CHECK(F1Udp::gt7SuggestedGear(0) == 15);
    CHECK((F1Udp::gt7Flags(car) & 1) == 1);
    CHECK((F1Udp::gt7Flags(car) & (1 << 5)) == 0);

    // Rev lights: the first lit at 11000, all lit at 11900.
    CHECK_NEAR(car.alertRpm, 11000, 0);
    CHECK(feed(d, telemetry(f, 300, 7, 11900, 100), 1010, car));
    CHECK_NEAR(car.shiftRpm, 11900, 0);
    CHECK((F1Udp::gt7Flags(car) & (1 << 5)) != 0);
    CHECK(feed(d, telemetry(f, 300, 7, 12100, 100), 1020, car));
    CHECK_NEAR(car.shiftRpm, 11900, 0);

    // The clock grows with the time and never repeats.
    const int32_t c1 = car.clock60;
    CHECK(feed(d, telemetry(f, 300, 7, 12100, 100), 1020, car));
    CHECK(car.clock60 == c1 + 1);
    CHECK(feed(d, telemetry(f, 300, 7, 12100, 100), 2020, car));
    CHECK(car.clock60 == 2020 * 3 / 50);

    // Laps: a valid lap becomes the best one, an invalid one does not.
    CHECK(!feed(d, lapData(f, 0, 90000, 1, false), 3000, car));
    CHECK(!feed(d, lapData(f, 91000, 100, 2, false), 3010, car));
    CHECK(feed(d, telemetry(f, 300, 7, 12000, 100), 3020, car));
    CHECK(car.lastLapMs == 91000 && car.bestLapMs == 91000 && car.lap == 2);
    CHECK(!feed(d, lapData(f, 91000, 80000, 2, true), 3100, car));
    CHECK(!feed(d, lapData(f, 85000, 100, 3, false), 3110, car));
    CHECK(feed(d, telemetry(f, 300, 7, 12000, 100), 3120, car));
    CHECK(car.lastLapMs == 85000 && car.bestLapMs == 91000);
    // The session history of the player wins; another car's is ignored.
    CHECK(!feed(d, history(f, PLAYER + 1, 1, 70000), 3200, car));
    CHECK(!feed(d, history(f, PLAYER, 2, 90500), 3210, car));
    CHECK(feed(d, telemetry(f, 300, 7, 12000, 100), 3220, car));
    CHECK(car.bestLapMs == 90500);

    // Paused, then in the garage: not on track.
    CHECK(!feed(d, session(f, 15, 44, true), 3300, car));
    CHECK(feed(d, telemetry(f, 0, 0, 4000, 0), 3310, car));
    CHECK(car.paused && !car.onTrack && (F1Udp::gt7Flags(car) & 2));
    CHECK(!feed(d, session(f, 15, 44, false), 3400, car));
    CHECK(!feed(d, lapData(f, 0, 0, 1, false, 0), 3410, car));
    CHECK(feed(d, telemetry(f, 0, 0, 4000, 0), 3420, car));
    CHECK(!car.paused && !car.onTrack);

    // Practice: no laps to go.
    CHECK(!feed(d, session(f, 1, 44, false), 3500, car));
    CHECK(feed(d, telemetry(f, 0, 0, 4000, 0), 3510, car));
    CHECK(car.totalLaps == 0);

    // Another team: the rev lights are learned again.
    CHECK(!feed(d, participants(f, 2), 3600, car));
    CHECK(feed(d, telemetry(f, 200, 5, 9000, 0), 3610, car));
    CHECK(car.teamId == 2);
    CHECK_NEAR(car.shiftRpm, 13000 * 0.95, 1);

    // A new session clears the lap times.
    Builder fresh = telemetry(f, 100, 3, 8000, 0);
    fresh.put<uint64_t>(7, SESSION + 1);
    CHECK(feed(d, fresh, 3700, car));
    CHECK(car.bestLapMs == -1 && car.lastLapMs == -1 && car.teamId == 2);

    // Spectating, other games, short packets: ignored.
    Builder spectator = telemetry(f, 100, 3, 8000, 0);
    spectator.b[27] = 255;
    CHECK(!feed(d, spectator, 3800, car));
    Builder f123 = telemetry(f, 100, 3, 8000, 0);
    f123.put<uint16_t>(0, 2023);
    CHECK(!feed(d, f123, 3800, car));
    Builder shortPacket = telemetry(f, 100, 3, 8000, 0);
    shortPacket.b.resize(shortPacket.b.size() - 10);
    CHECK(!feed(d, shortPacket, 3800, car));
    CHECK(!d.feed(shortPacket.b.data(), 10, 3800, car));
}

int main()
{
    testFormat(F2025);
    testFormat(F2026);
    if (failures) { printf("%d failures\n", failures); return 1; }
    printf("All F1 decoder tests passed\n");
    return 0;
}
