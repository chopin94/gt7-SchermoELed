// Tests of src/RaceTelemetry.h and src/JsonOut.h: a simulated race of three
// laps through the lap analysis of the strip.
//   g++ -std=c++11 -Wall -Wextra -pedantic firmware/led-strip/tests/race_telemetry.cpp -o /tmp/race-tests && /tmp/race-tests
#include "sim_race.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
    } while (0)

// ---- A strict JSON parser, enough to check what the page receives ----
struct JsonCheck
{
    const char *p;
    bool ok = true;
    explicit JsonCheck(const char *text) : p(text) {}
    void ws() { while (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r') ++p; }
    bool literal(const char *w) { size_t n = strlen(w); if (strncmp(p, w, n)) return false; p += n; return true; }
    void value()
    {
        ws();
        if (*p == '{')
        {
            ++p; ws();
            if (*p == '}') { ++p; return; }
            for (;;)
            {
                ws(); str(); ws();
                if (*p++ != ':') { ok = false; return; }
                value(); ws();
                if (*p == ',') { ++p; continue; }
                if (*p == '}') { ++p; return; }
                ok = false; return;
            }
        }
        if (*p == '[')
        {
            ++p; ws();
            if (*p == ']') { ++p; return; }
            for (;;)
            {
                value(); ws();
                if (*p == ',') { ++p; continue; }
                if (*p == ']') { ++p; return; }
                ok = false; return;
            }
        }
        if (*p == '"') { str(); return; }
        if (literal("null") || literal("true") || literal("false")) return;
        number();
    }
    void str()
    {
        if (*p != '"') { ok = false; return; }
        ++p;
        while (*p && *p != '"') { if (*p == '\\') ++p; if (*p) ++p; }
        if (*p != '"') { ok = false; return; }
        ++p;
    }
    void number()
    {
        const char *start = p;
        if (*p == '-') ++p;
        if (!(*p >= '0' && *p <= '9')) { ok = false; return; }
        while (*p >= '0' && *p <= '9') ++p;
        if (*p == '.') { ++p; if (!(*p >= '0' && *p <= '9')) { ok = false; return; } while (*p >= '0' && *p <= '9') ++p; }
        if (p == start) ok = false;
    }
    bool valid() { value(); ws(); return ok && *p == 0; }
};
static bool validJson(const char *text) { JsonCheck c(text); return c.valid(); }

static void testJsonOut()
{
    char buf[256];
    JsonOut j(buf, sizeof(buf));
    j.beginObject();
    j.field("a", 12LL);
    j.field("b", -3.14159f, 2);
    j.field("c", NAN, 1);
    j.field("d", 0.05f, 1);
    j.field("e", -0.04f, 1);
    j.key("f").beginArray().integer(1).number(2.5f, 1).null().beginObject().text("s", "x\"y").endObject().endArray();
    j.flag("g", true);
    j.endObject();
    CHECK(j.ok());
    CHECK(std::string(buf) == "{\"a\":12,\"b\":-3.14,\"c\":null,\"d\":0.1,\"e\":0.0,\"f\":[1,2.5,null,{\"s\":\"x\\\"y\"}],\"g\":1}");
    CHECK(validJson(buf));
    char tiny[8];
    JsonOut t(tiny, sizeof(tiny));
    t.beginObject().field("long_key", 1LL).endObject();
    CHECK(!t.ok() && strlen(tiny) < sizeof(tiny));
}

static void testFuel()
{
    Race::FuelEstimator f;
    f.reset();
    f.update(50, 0);
    f.update(49, 1); // the start: not a lap
    CHECK(f.lapsLeft() < 0);
    f.update(46, 2);
    CHECK(fabsf(f.usedPerLap() - 3.0f) < 0.01f && fabsf(f.lapsLeft() - 46.0f / 3.0f) < 0.01f);
    f.update(80, 2); // refuelled in the pits
    f.update(77, 3);
    CHECK(fabsf(f.usedPerLap() - 3.0f) < 0.01f);
}

static void testRace()
{
    SimRace sim;
    Race::Telemetry race;
    int best = 0, sectors = 0, finalLap = 0, finish = 0;
    bool deltaOnSecondLap = false, brakePointSeen = false;
    int sectorColours[3] = {0, 0, 0};
    std::vector<char> big(16384);
    char frame[2048];
    size_t largestFrame = 0;
    for (int k = 0; k < 60 * 400 && sim.lapCount <= 4; ++k)
    {
        const Race::Packet p = sim.step();
        race.update(p, sim.nowMs);
        LedFx::Event e;
        while (race.takeEvent(e))
        {
            if (e == LedFx::Event::BestLap) best++;
            if (e == LedFx::Event::FinalLap) finalLap++;
            if (e == LedFx::Event::Finish) finish++;
            if (e == LedFx::Event::SectorPurple) { sectors++; sectorColours[0]++; }
            if (e == LedFx::Event::SectorGreen) { sectors++; sectorColours[1]++; }
            if (e == LedFx::Event::SectorYellow) { sectors++; sectorColours[2]++; }
        }
        LedFx::Input in;
        race.fillInput(in, sim.nowMs);
        CHECK(in.racing);
        if (sim.lapCount == 2 && sim.lapFraction() > 0.2f && in.hasDelta) deltaOnSecondLap = true;
        if (in.hasBrakePoint && in.brakePointM < 150) brakePointSeen = true;
        // Lap 2 is slower in its second half, lap 3 faster.
        sim.pace = sim.lapCount == 2 && sim.lapFraction() > 0.5f ? 0.93f : sim.lapCount == 3 ? 1.0f : 0.97f;
        if (k % 20 == 0)
        {
            JsonOut j(frame, sizeof(frame));
            race.writeFrame(j, sim.nowMs);
            CHECK(j.ok());
            if (j.length() > largestFrame) largestFrame = j.length();
            if (k % 2000 == 0) CHECK(validJson(frame));
        }
    }
    printf("  laps %d, frame up to %zu bytes, packets %.0f/s\n", sim.lapCount, largestFrame, race.packetRate(sim.nowMs));
    CHECK(largestFrame < 1500);
    CHECK(race.lap.mapState() == LapAnalysis::MapState::Complete);
    CHECK(race.lap.lapsCompleted() == 4);
    CHECK(deltaOnSecondLap);
    CHECK(brakePointSeen);
    CHECK(best >= 2);         // lap 1, then the fast lap 3
    CHECK(finalLap == 1);     // lap 3 of 3 starts
    CHECK(finish == 1);       // lap counter 4 > 3
    CHECK(sectors >= 6);      // lap 1 at the line, then each sector of the next laps
    CHECK(sectorColours[0] > 0 && sectorColours[2] > 0);
    CHECK(race.fuelPerLap() > 0 && race.fuelLapsLeft() > 0);

    JsonOut j(big.data(), big.size());
    race.writeSession(j);
    CHECK(j.ok());
    CHECK(validJson(big.data()));
    printf("  session %zu bytes\n", j.length());
    CHECK(strstr(big.data(), "\"laps\":[{\"n\":4") != nullptr);

    // A stop: paused, then no packets at all.
    Race::Packet p = sim.packet();
    p.packetId++;
    p.flags |= Race::FLAG_PAUSED;
    race.update(p, sim.nowMs + 20);
    CHECK(!race.racing(sim.nowMs + 20) && race.live(sim.nowMs + 20));
    CHECK(!race.live(sim.nowMs + 2000));

    // Reset of the session.
    race.resetSession();
    CHECK(race.lap.lapsCompleted() == 0 && race.lap.mapState() == LapAnalysis::MapState::Waiting);
}

// ---- The same race sent by F1 25 (UDP format 2025), byte by byte ----
struct F1Wire
{
    static const int CARS = 22, PLAYER = 5;
    std::vector<uint8_t> b;
    F1Wire(uint8_t id, size_t size) : b(size, 0)
    {
        put<uint16_t>(0, 2025);
        b[2] = 25;
        b[6] = id;
        put<uint64_t>(7, 42);
        b[27] = PLAYER;
    }
    template <typename T>
    void put(size_t offset, T v) { memcpy(&b[offset], &v, sizeof(T)); }
};

// One frame of the simulated race as the packets of F1 25.
static std::vector<F1Wire> f1Frame(const SimRace &sim)
{
    const Race::Packet g = sim.packet();
    std::vector<F1Wire> out;
    F1Wire motion(0, 29 + F1Wire::CARS * 60);
    size_t o = 29 + F1Wire::PLAYER * 60;
    for (int i = 0; i < 3; ++i)
    {
        motion.put<float>(o + i * 4, g.position[i]);
        motion.put<float>(o + 12 + i * 4, g.velocity[i]);
    }
    out.push_back(motion);
    F1Wire session(1, 753);
    session.b[29 + 3] = static_cast<uint8_t>(sim.totalLaps + 1); // the out-lap is lap 1
    session.b[29 + 6] = 15;                                       // race
    out.push_back(session);
    F1Wire lap(2, 29 + F1Wire::CARS * 57 + 2);
    o = 29 + F1Wire::PLAYER * 57;
    lap.put<uint32_t>(o, g.lastLapMs > 0 ? g.lastLapMs : 0);
    lap.put<uint32_t>(o + 4, g.currentLapMs > 0 ? g.currentLapMs : 0);
    lap.b[o + 32] = 4;
    lap.b[o + 33] = static_cast<uint8_t>(sim.lapCount + 1);
    lap.b[o + 44] = 4; // on track
    out.push_back(lap);
    F1Wire status(7, 29 + F1Wire::CARS * 55);
    o = 29 + F1Wire::PLAYER * 55;
    status.put<float>(o + 5, g.fuelLevel);
    status.put<float>(o + 9, g.fuelCapacity);
    status.put<uint16_t>(o + 17, 8400);
    out.push_back(status);
    F1Wire ex(13, 273);
    for (int i = 0; i < 4; ++i) ex.put<float>(29 + 48 + i * 4, g.speed);
    out.push_back(ex);
    F1Wire tel(6, 29 + F1Wire::CARS * 60 + 3);
    o = 29 + F1Wire::PLAYER * 60;
    tel.put<uint16_t>(o, static_cast<uint16_t>(lroundf(g.speed * 3.6f)));
    tel.put<float>(o + 2, g.throttle / 255.0f);
    tel.put<float>(o + 10, g.brake / 255.0f);
    tel.b[o + 15] = g.gear;
    tel.put<uint16_t>(o + 16, static_cast<uint16_t>(g.rpm));
    tel.b[o + 19] = g.rpm >= 7000 ? static_cast<uint8_t>(fminf(100.0f, (g.rpm - 7000) / 10.0f)) : 0;
    out.push_back(tel);
    return out;
}

static void testF1Race()
{
    SimRace sim;
    Race::Telemetry race;
    F1Udp::Decoder decoder;
    int best = 0, finalLap = 0, finish = 0, samples = 0;
    bool deltaOnSecondLap = false, racing = true;
    for (int k = 0; k < 60 * 400 && sim.lapCount <= 4; ++k)
    {
        sim.step();
        for (const F1Wire &w : f1Frame(sim))
        {
            F1Udp::Car car;
            if (!decoder.feed(w.b.data(), w.b.size(), sim.nowMs, car)) continue;
            Race::Packet p;
            Race::fromF1(car, p);
            race.update(p, sim.nowMs);
            samples++;
        }
        LedFx::Event e;
        while (race.takeEvent(e))
        {
            if (e == LedFx::Event::BestLap) best++;
            if (e == LedFx::Event::FinalLap) finalLap++;
            if (e == LedFx::Event::Finish) finish++;
        }
        LedFx::Input in;
        race.fillInput(in, sim.nowMs);
        if (k > 2) racing = racing && in.racing;
        if (sim.lapCount == 2 && sim.lapFraction() > 0.2f && in.hasDelta) deltaOnSecondLap = true;
        sim.pace = sim.lapCount == 2 && sim.lapFraction() > 0.5f ? 0.93f : sim.lapCount == 3 ? 1.0f : 0.97f;
    }
    printf("  F1 25: %d samples, laps %d, packets %.0f/s\n", samples, sim.lapCount, race.packetRate(sim.nowMs));
    CHECK(racing);
    CHECK(race.lap.mapState() == LapAnalysis::MapState::Complete);
    CHECK(race.lap.lapsCompleted() == 4);
    CHECK(deltaOnSecondLap);
    CHECK(best >= 2);
    CHECK(finalLap == 1);
    CHECK(finish == 1);
    CHECK(race.packet().maxAlertRpm > 7000 && race.packet().minAlertRpm >= 7000);
    CHECK((race.packet().carCode >> 16) == 0); // no participants packet: no team yet
}

int main()
{
    testJsonOut();
    testFuel();
    testRace();
    testF1Race();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    printf("race telemetry: all checks passed\n");
    return 0;
}
