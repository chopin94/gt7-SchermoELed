// Tests of the persisted circuit and reference lap in src/LapAnalysis.h:
//   g++ -std=c++11 -Wall -Wextra -pedantic tests/track_blobs.cpp -o /tmp/blob-tests && /tmp/blob-tests
#include "stadium.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

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

struct Saved
{
    std::vector<uint8_t> track, reference;
    uint32_t key = 0;
    int32_t refLapMs = 0;
};

// A first session: the circuit and the reference lap are recorded and the
// blocks are collected as the analyzer asks for them.
static Saved firstSession(float offsetX = 0)
{
    Saved saved;
    Analyzer &a = *new Analyzer();
    Driver d(a);
    d.offsetX = offsetX;
    d.s = 1400;
    d.driveToLap(2);
    saved.key = a.trackKey();
    saved.refLapMs = a.referenceLapMs();
    SaveKind kind;
    std::vector<uint8_t> buffer(REFERENCE_BLOB_MAX);
    size_t length = 0;
    while (a.takeSaveRequest(kind))
    {
        if (kind == SaveKind::Track)
        {
            if (a.exportTrack(buffer.data(), buffer.size(), length)) saved.track.assign(buffer.begin(), buffer.begin() + length);
        }
        else if (a.exportReference(buffer.data(), buffer.size(), length))
            saved.reference.assign(buffer.begin(), buffer.begin() + length);
    }
    delete &a;
    return saved;
}

static void testSaveRequests()
{
    Analyzer &a = *new Analyzer();
    Driver d(a);
    SaveKind kind;
    d.s = 1400;
    d.driveToLap(1);
    // The map is still being recorded: nothing to save.
    CHECK(a.trackKey() == 0);
    CHECK(!a.takeSaveRequest(kind));
    uint8_t buffer[REFERENCE_BLOB_MAX];
    size_t length = 0;
    CHECK(!a.exportTrack(buffer, sizeof(buffer), length));
    CHECK(!a.exportReference(buffer, sizeof(buffer), length));
    d.driveToLap(2);
    CHECK(a.trackKey() != 0);
    CHECK(a.takeSaveRequest(kind) && kind == SaveKind::Track);
    CHECK(a.takeSaveRequest(kind) && kind == SaveKind::Reference);
    CHECK(!a.takeSaveRequest(kind));
    CHECK(a.exportTrack(buffer, sizeof(buffer), length) && length == TRACK_BLOB_SIZE);
    CHECK(!a.exportTrack(buffer, TRACK_BLOB_SIZE - 1, length)); // too small
    CHECK(a.exportReference(buffer, sizeof(buffer), length) && length <= REFERENCE_BLOB_MAX);

    // A slower lap does not ask for a save; a faster one asks for the reference only.
    d.pace = 0.97f;
    d.driveToLap(3);
    CHECK(!a.takeSaveRequest(kind));
    d.pace = 1.02f;
    d.driveToLap(4);
    CHECK(a.takeSaveRequest(kind) && kind == SaveKind::Reference);
    CHECK(!a.takeSaveRequest(kind));
    // A new circuit starts from nothing.
    a.reset();
    CHECK(a.trackKey() == 0 && !a.takeSaveRequest(kind));
    delete &a;
}

static void testRecognisesTheCircuit()
{
    const Saved saved = firstSession();
    CHECK(saved.track.size() == TRACK_BLOB_SIZE && saved.key != 0);
    const uint8_t *t = saved.track.data();
    const size_t n = saved.track.size();
    CHECK(Analyzer::trackBlobMatches(t, n, 200, 0, 1, 0));       // top straight, going on
    CHECK(Analyzer::trackBlobMatches(t, n, 300, 2, 1, 0.05f));   // a few metres off the line
    CHECK(Analyzer::trackBlobMatches(t, n, 300, 200, -1, 0));    // bottom straight
    CHECK(Analyzer::trackBlobMatches(t, n, 600, 40, 0.5f, 0.8f)); // in the first corner
    CHECK(!Analyzer::trackBlobMatches(t, n, 200, 0, -1, 0));     // the same road backwards
    CHECK(!Analyzer::trackBlobMatches(t, n, 300, 200, 1, 0));
    CHECK(!Analyzer::trackBlobMatches(t, n, 200, 80, 1, 0));     // 80 m away
    CHECK(!Analyzer::trackBlobMatches(t, n, 5000, 5000, 1, 0));
    CHECK(!Analyzer::trackBlobMatches(t, n - 1, 200, 0, 1, 0));  // truncated
    CHECK(!Analyzer::trackBlobMatches(nullptr, 0, 200, 0, 1, 0));
}

static void testRestoreInANewSession()
{
    const Saved saved = firstSession();
    Analyzer &a = *new Analyzer();
    SaveKind kind;
    CHECK(a.wantsTrack());
    CHECK(a.importTrack(saved.track.data(), saved.track.size()));
    CHECK(a.mapState() == MapState::Complete);
    CHECK(a.trackKey() == saved.key);
    CHECK(!a.wantsTrack());
    CHECK(a.importReference(saved.reference.data(), saved.reference.size()));
    CHECK(a.referenceLapMs() == saved.refLapMs);
    CHECK(a.referenceBrakeZoneCount() == 2);
    CHECK(a.sectorsAvailable());
    CHECK(!a.takeSaveRequest(kind)); // what was just loaded is not saved again
    // The map is the one that was saved.
    float minX, minZ, maxX, maxZ;
    CHECK(a.mapBounds(minX, minZ, maxX, maxZ));
    CHECK_NEAR(minX, -RADIUS, 15.0); CHECK_NEAR(maxX, STRAIGHT + RADIUS, 15.0);
    CHECK(a.mapPointCount() == MAP_POINTS);

    // Lap 1 of the new session already has a delta, sectors and braking points.
    Driver d(a);
    d.s = 1400;
    d.driveToLap(1);
    d.driveTo(300);
    CHECK(a.hasLiveDelta());
    CHECK_NEAR(a.liveDeltaMs(), 0, 150);
    float meters, kmh;
    CHECK(a.nextBrakePoint(meters, kmh));
    CHECK_NEAR(meters, 366.0 - 300.0, 8.0);
    d.driveTo(450);
    CHECK_NEAR(a.brakeDelta(0), 0.0, 3.0);
    d.driveToLap(2);
    CHECK(a.lastLapSectorState(0) != SectorState::None);
    CHECK(a.referenceLapMs() > 0);
    delete &a;
}

static void testSessionBestStaysTheSession()
{
    const Saved saved = firstSession();
    Analyzer &a = *new Analyzer();
    Driver d(a);
    d.s = 1400;
    d.driveToLap(0); // out-lap, nothing yet
    CHECK(a.importTrack(saved.track.data(), saved.track.size()));
    CHECK(a.importReference(saved.reference.data(), saved.reference.size()));
    // The best lap of the session is not the saved one.
    CHECK(a.bestLapMs() < 0);
    d.driveToLap(2);
    CHECK(a.bestLapMs() > 0);
    delete &a;
}

static void testRejectsDamagedBlocks()
{
    const Saved saved = firstSession();
    // Any single changed byte, anywhere, is caught by the CRC.
    for (size_t i = 0; i < saved.track.size(); i += 37)
    {
        std::vector<uint8_t> bad = saved.track;
        bad[i] ^= 0x40;
        Analyzer &a = *new Analyzer();
        CHECK(!a.importTrack(bad.data(), bad.size()));
        CHECK(a.mapState() == MapState::Waiting);
        delete &a;
    }
    for (size_t i = 0; i < saved.reference.size(); i += 97)
    {
        Analyzer &a = *new Analyzer();
        CHECK(a.importTrack(saved.track.data(), saved.track.size()));
        std::vector<uint8_t> bad = saved.reference;
        bad[i] ^= 0x01;
        CHECK(!a.importReference(bad.data(), bad.size()));
        CHECK(a.referenceLapMs() < 0 && a.referenceBrakeZoneCount() == 0);
        delete &a;
    }
    // Cut short by a power loss.
    for (size_t cut = 1; cut < 40; cut += 7)
    {
        Analyzer &a = *new Analyzer();
        CHECK(!a.importTrack(saved.track.data(), saved.track.size() - cut));
        CHECK(a.importTrack(saved.track.data(), saved.track.size()));
        CHECK(!a.importReference(saved.reference.data(), saved.reference.size() - cut));
        delete &a;
    }
    CHECK(true);
}

static void testRejectsWhatDoesNotFit()
{
    const Saved saved = firstSession();
    const Saved other = firstSession(5000); // another circuit
    CHECK(other.key != saved.key);

    // A reference lap needs its circuit.
    {
        Analyzer &a = *new Analyzer();
        CHECK(!a.importReference(saved.reference.data(), saved.reference.size()));
        delete &a;
    }
    // ...and the right one.
    {
        Analyzer &a = *new Analyzer();
        CHECK(a.importTrack(other.track.data(), other.track.size()));
        CHECK(!a.importReference(saved.reference.data(), saved.reference.size()));
        CHECK(a.importReference(other.reference.data(), other.reference.size()));
        delete &a;
    }
    // A map is not loaded over another one.
    {
        Analyzer &a = *new Analyzer();
        CHECK(a.importTrack(saved.track.data(), saved.track.size()));
        CHECK(!a.importTrack(other.track.data(), other.track.size()));
        CHECK(a.trackKey() == saved.key);
        delete &a;
    }
    // A block of the distance kind does not load in a fraction source (AC).
    {
        Analyzer &a = *new Analyzer();
        Driver d(a);
        d.fractionMode = true;
        d.s = 1400;
        d.drive(30);
        CHECK(a.fractionMode());
        CHECK(!a.importTrack(saved.track.data(), saved.track.size()));
        delete &a;
    }
    // After a track change the old key is gone.
    {
        Analyzer &a = *new Analyzer();
        CHECK(a.importTrack(saved.track.data(), saved.track.size()));
        a.reset();
        CHECK(a.trackKey() == 0 && a.wantsTrack());
        CHECK(a.importTrack(other.track.data(), other.track.size()));
        delete &a;
    }
}

static void testFractionModeRoundTrip()
{
    // Assetto Corsa reports the fraction of the lap: its blocks load only there.
    Saved saved;
    {
        Analyzer &a = *new Analyzer();
        Driver d(a);
        d.fractionMode = true;
        d.s = 1400;
        d.driveToLap(2);
        std::vector<uint8_t> buffer(REFERENCE_BLOB_MAX);
        size_t length = 0;
        SaveKind kind;
        while (a.takeSaveRequest(kind))
        {
            if (kind == SaveKind::Track && a.exportTrack(buffer.data(), buffer.size(), length))
                saved.track.assign(buffer.begin(), buffer.begin() + length);
            if (kind == SaveKind::Reference && a.exportReference(buffer.data(), buffer.size(), length))
                saved.reference.assign(buffer.begin(), buffer.begin() + length);
        }
        delete &a;
    }
    CHECK(!saved.track.empty() && !saved.reference.empty());
    Analyzer &a = *new Analyzer();
    Driver d(a);
    d.fractionMode = true;
    d.s = 1400;
    d.drive(30);
    CHECK(a.importTrack(saved.track.data(), saved.track.size()));
    CHECK(a.importReference(saved.reference.data(), saved.reference.size()));
    d.driveToLap(1);
    d.driveTo(300);
    CHECK(a.hasLiveDelta());
    delete &a;
    // The same blocks do not load in a GT7 (distance) analyzer.
    Analyzer &g = *new Analyzer();
    CHECK(!g.importTrack(saved.track.data(), saved.track.size()));
    delete &g;
}

static void testCrc()
{
    const uint8_t text[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK(crc32(text, sizeof(text)) == 0xCBF43926u); // the standard check value
}

int main()
{
    testCrc();
    testSaveRequests();
    testRecognisesTheCircuit();
    testRestoreInANewSession();
    testSessionBestStaysTheSession();
    testRejectsDamagedBlocks();
    testRejectsWhatDoesNotFit();
    testFractionModeRoundTrip();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    printf("circuit blocks: all checks passed\n");
    return 0;
}
