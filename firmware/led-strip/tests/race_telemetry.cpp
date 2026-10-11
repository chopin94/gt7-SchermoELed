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

int main()
{
    testJsonOut();
    testFuel();
    testRace();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    printf("race telemetry: all checks passed\n");
    return 0;
}
