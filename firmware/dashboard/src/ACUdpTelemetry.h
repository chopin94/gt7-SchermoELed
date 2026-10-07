#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>
#include <string.h>

// Assetto Corsa "Remote Telemetry" UDP protocol, served by the PC game and by
// the PS4 console edition (also when played on PS5) while a session is loaded.
// The game listens on UDP 9996: a client sends a handshake, receives the car
// and track names, then subscribes to one RTCarInfo packet per update.
// The console is found by broadcasting the handshake and, for hosts that
// ignore broadcasts, by asking the addresses of the local network in turn;
// the reply reveals the address to subscribe to.
class ACUdpTelemetry {
public:
    struct CarInfo {
        float speedKmh = 0;
        bool absEnabled = false, absInAction = false;
        bool tcEnabled = false, tcInAction = false;
        bool inPit = false, engineLimiterOn = false;
        int32_t lapTimeMs = 0, lastLapMs = 0, bestLapMs = 0, lapCount = 0;
        float gas = 0, brake = 0, clutch = 0, engineRpm = 0;
        int32_t gear = 1; // 0 = reverse, 1 = neutral, 2 = first gear
        float lapPosition = 0; // 0-1 along the lap
        float x = 0, y = 0, z = 0; // world position in metres (y is up)
    };

    void begin() {
        udp.begin(LOCAL_PORT);
        server = IPAddress();
        subscribed = false;
        lastHandshake = lastData = 0;
    }

    void stop() {
        if (subscribed) send(server, OP_DISMISS);
        udp.stop();
        subscribed = false;
        server = IPAddress();
    }

    // True once after a handshake reports a different car than before.
    bool takeCarChanged() { const bool changed = carChanged; carChanged = false; return changed; }
    // True once after a handshake reports a different track or layout.
    bool takeTrackChanged() { const bool changed = trackChanged; trackChanged = false; return changed; }

    // Drains pending packets and keeps the subscription alive. Returns true
    // when `info` holds a new car update.
    bool poll(uint32_t now, CarInfo &info) {
        bool updated = false;
        for (int budget = 16; budget > 0; --budget) {
            const int size = udp.parsePacket();
            if (size <= 0) break;
            uint8_t buffer[HANDSHAKE_RESPONSE_SIZE];
            const int length = udp.read(buffer, sizeof(buffer));
            if (size == HANDSHAKE_RESPONSE_SIZE && length == HANDSHAKE_RESPONSE_SIZE) {
                handleHandshakeResponse(buffer, now);
            } else if (subscribed && length >= CAR_INFO_SIZE) {
                parseCarInfo(buffer, info);
                lastData = now;
                updated = true;
            }
        }
        // A new session or a return to the menus ends the stream: start over.
        if (subscribed && uint32_t(now - lastData) > DATA_TIMEOUT_MS) {
            subscribed = false;
            server = IPAddress();
        }
        if (!subscribed && uint32_t(now - lastHandshake) >= HANDSHAKE_INTERVAL_MS) {
            lastHandshake = now;
            if (lastServer != IPAddress()) send(lastServer, OP_HANDSHAKE);
            send(IPAddress(255, 255, 255, 255), OP_HANDSHAKE);
            const IPAddress subnetBroadcast = WiFi.broadcastIP();
            if (subnetBroadcast != IPAddress() && subnetBroadcast != IPAddress(255, 255, 255, 255))
                send(subnetBroadcast, OP_HANDSHAKE);
            sweepLocalNetwork();
        }
        return updated;
    }

private:
    static constexpr uint16_t SERVER_PORT = 9996;
    static constexpr uint16_t LOCAL_PORT = 9997;
    static constexpr int32_t DEVICE_IDENTIFIER = 1; // client device type (1 = phone)
    static constexpr int32_t PROTOCOL_VERSION = 1;
    static constexpr int32_t OP_HANDSHAKE = 0;
    static constexpr int32_t OP_SUBSCRIBE_UPDATE = 1;
    static constexpr int32_t OP_DISMISS = 3;
    static constexpr int HANDSHAKE_RESPONSE_SIZE = 408;
    static constexpr int CAR_INFO_SIZE = 328;
    static constexpr uint32_t HANDSHAKE_INTERVAL_MS = 1000;
    static constexpr uint32_t DATA_TIMEOUT_MS = 3000;
    // Unicast handshakes per interval: a /24 network is covered in ~8 s.
    static constexpr int SWEEP_PER_INTERVAL = 32;

    WiFiUDP udp;
    IPAddress server, lastServer;
    bool subscribed = false;
    bool carChanged = false, trackChanged = false;
    uint32_t lastHandshake = 0, lastData = 0;
    uint16_t sweepHost = 1;
    char carName[51] = {};
    char trackName[104] = {}; // "track|layout"

    // Asks the next few addresses of the local network (limited to the /24
    // around this device) for a handshake.
    void sweepLocalNetwork() {
        const IPAddress local = WiFi.localIP();
        const IPAddress mask = WiFi.subnetMask();
        uint16_t first = 1, last = 254;
        if (mask[0] == 255 && mask[1] == 255 && mask[2] == 255) {
            const uint8_t network = local[3] & mask[3];
            first = network + 1;
            last = network + static_cast<uint8_t>(~mask[3]) - 1;
        }
        for (int i = 0; i < SWEEP_PER_INTERVAL; ++i) {
            if (sweepHost < first || sweepHost > last) sweepHost = first;
            const IPAddress target(local[0], local[1], local[2], static_cast<uint8_t>(sweepHost++));
            if (target != local) send(target, OP_HANDSHAKE);
        }
    }

    void send(const IPAddress &address, int32_t operation) {
        const int32_t packet[3] = {DEVICE_IDENTIFIER, PROTOCOL_VERSION, operation};
        udp.beginPacket(address, SERVER_PORT);
        udp.write(reinterpret_cast<const uint8_t *>(packet), sizeof(packet));
        udp.endPacket();
    }

    // Copies a UTF-16LE string of 50 characters as ASCII.
    static void readName(const uint8_t *buffer, char *out) {
        for (int i = 0; i < 50; ++i) {
            const uint16_t character = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
            out[i] = character == 0 ? 0 : character < 128 ? static_cast<char>(character) : '?';
            if (character == 0) return;
        }
        out[50] = 0;
    }

    void handleHandshakeResponse(const uint8_t *buffer, uint32_t now) {
        // Response: carName[50], driverName[50] (UTF-16LE), identifier,
        // version, trackName[50], trackConfig[50].
        char name[51] = {};
        readName(buffer, name);
        if (strcmp(name, carName) != 0) {
            strcpy(carName, name);
            carChanged = true;
        }
        char track[104] = {};
        readName(buffer + 208, track);
        const size_t length = strlen(track);
        track[length] = '|';
        readName(buffer + 308, track + length + 1);
        if (strcmp(track, trackName) != 0) {
            strcpy(trackName, track);
            trackChanged = true;
        }
        // Several handshakes may be answered (broadcast and sweep): subscribe once.
        if (subscribed) return;
        server = lastServer = udp.remoteIP();
        send(server, OP_SUBSCRIBE_UPDATE);
        subscribed = true;
        lastData = now;
    }

    template <typename T>
    static T field(const uint8_t *buffer, size_t offset) {
        T value;
        memcpy(&value, buffer + offset, sizeof(T));
        return value;
    }

    // RTCarInfo layout (little-endian, 4-byte aligned, 328 bytes).
    static void parseCarInfo(const uint8_t *buffer, CarInfo &info) {
        info.speedKmh = field<float>(buffer, 8);
        info.absEnabled = buffer[20] != 0;
        info.absInAction = buffer[21] != 0;
        info.tcInAction = buffer[22] != 0;
        info.tcEnabled = buffer[23] != 0;
        info.inPit = buffer[24] != 0;
        info.engineLimiterOn = buffer[25] != 0;
        info.lapTimeMs = field<int32_t>(buffer, 40);
        info.lastLapMs = field<int32_t>(buffer, 44);
        info.bestLapMs = field<int32_t>(buffer, 48);
        info.lapCount = field<int32_t>(buffer, 52);
        info.gas = field<float>(buffer, 56);
        info.brake = field<float>(buffer, 60);
        info.clutch = field<float>(buffer, 64);
        info.engineRpm = field<float>(buffer, 68);
        info.gear = field<int32_t>(buffer, 76);
        info.lapPosition = field<float>(buffer, 308);
        info.x = field<float>(buffer, 316);
        info.y = field<float>(buffer, 320);
        info.z = field<float>(buffer, 324);
    }
};
