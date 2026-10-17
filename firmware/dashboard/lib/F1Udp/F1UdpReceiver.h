#pragma once
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "F1Decoder.h"

// Listens to F1 25 on UDP 20777 (broadcast or sent to this address) and
// hands over the player's car, one sample per car telemetry packet.
// Screen and LED strip pass the car on to each other (setRelay): then F1 25
// may send to one address alone, without broadcast, which some networks do
// not carry. Only the decoded car travels (one small datagram per sample,
// instead of the ~7 large packets of the game for 22 cars), to RELAY_PORT,
// so each device tells it from what it receives itself and uses it only
// when nothing arrives directly. The screen says hello on RELAY_PORT every
// two seconds, so the strip learns where the screen is (peer()).
class F1UdpReceiver
{
public:
    static constexpr uint16_t RELAY_PORT = 20778;

    explicit F1UdpReceiver(uint16_t listenPort = F1Udp::PORT) : port(listenPort) {}
    void begin() { udp.begin(port); }
    void stop() { udp.stop(); }
    // Where to copy the decoded car (none: IPAddress()).
    void setRelay(const IPAddress &address) { relay = address; }
    // The screen to the strip: "here I am, copy F1 25 to me too".
    void sayHello(const IPAddress &address)
    {
        udp.beginPacket(address, RELAY_PORT);
        udp.write(reinterpret_cast<const uint8_t *>(hello()), HELLO_LENGTH);
        udp.endPacket();
    }
    // Who said hello in the last 10 seconds (IPAddress() if nobody).
    IPAddress peer() const { return peerSeen && millis() - peerMs < 10000 ? peerAddress : IPAddress(); }
    // The console that sends F1 25 here (IPAddress() until it does): GT7
    // runs on the same one, its heartbeat can go there directly.
    IPAddress gameAddress() const { return gameSender; }

    // Reads the waiting datagrams until a new sample is complete. Call it
    // again until it returns false: the lap analysis wants every sample.
    bool poll(F1Udp::Car &car)
    {
        for (int budget = 24; budget > 0; --budget)
        {
            const int size = udp.parsePacket();
            if (size <= 0) return false;
            const int length = udp.read(buffer, sizeof(buffer));
            if (length <= 0 || size > static_cast<int>(sizeof(buffer))) continue;
            if (length == HELLO_LENGTH && memcmp(buffer, hello(), HELLO_LENGTH) == 0)
            {
                peerAddress = udp.remoteIP();
                peerMs = millis();
                peerSeen = true;
                continue;
            }
            countDatagram(length);
            if (length == RELAYED_SIZE && memcmp(buffer, relayedMagic(), 4) == 0)
            {
                // A car decoded by the other device: our own clock.
                memcpy(&car, buffer + 4, sizeof(F1Udp::Car));
                const int32_t tick = static_cast<int32_t>(static_cast<uint64_t>(millis()) * 3 / 50);
                car.clock60 = relayClockStarted && tick <= relayClock ? relayClock + 1 : tick;
                relayClock = car.clock60;
                relayClockStarted = true;
                lastFormat = 2025;
                return accept(car);
            }
            if (length >= 2) lastFormat = buffer[0] | (buffer[1] << 8);
            gameSender = udp.remoteIP();
            if (decoder.feed(buffer, static_cast<size_t>(length), millis(), car))
            {
                if (relay != IPAddress() && gameSender != relay)
                {
                    uint8_t out[RELAYED_SIZE];
                    memcpy(out, relayedMagic(), 4);
                    memcpy(out + 4, &car, sizeof(F1Udp::Car));
                    udp.beginPacket(relay, RELAY_PORT);
                    udp.write(out, sizeof(out));
                    udp.endPacket();
                }
                return accept(car);
            }
        }
        return false;
    }

    // F1 data in the last `ms` milliseconds.
    bool live(uint32_t ms = 2000) const { return received && millis() - lastSampleMs < ms; }

    // What arrives on the port, in words, for the waiting screen and the
    // web page: shows at once whether the game is sending and what.
    void describe(char *out, size_t size)
    {
        countDatagram(0);
        const bool recent = datagramsEver && millis() - lastDatagramMs < 3000;
        if (!datagramsEver || !recent)
            snprintf(out, size, "F1 25 nessun dato - IP %s porta %u", WiFi.localIP().toString().c_str(), port);
        else if (lastFormat < 2024 || lastFormat > 2026)
            snprintf(out, size, "F1 25: formato UDP %u, scegli 2025", lastFormat);
        else if (!live(3000))
            snprintf(out, size, "F1 25: %u pacchetti/s, nessuna auto", static_cast<unsigned>(rate));
        else if (lastPaused)
            snprintf(out, size, "F1 25: %u pacchetti/s, in pausa", static_cast<unsigned>(rate));
        else if (!lastOnTrack)
            snprintf(out, size, "F1 25: %u pacchetti/s, auto ai box", static_cast<unsigned>(rate));
        else
            snprintf(out, size, "F1 25: %u pacchetti/s, in pista", static_cast<unsigned>(rate));
    }

private:
    static const char *hello() { return "GT7DASH-F1"; }
    static const int HELLO_LENGTH = 10;
    // "F1C" + layout version, then the Car as it is in memory: screen and
    // strip are both Xtensa ESP32 builds of this same header.
    static const char *relayedMagic() { return "F1C1"; }
    static const int RELAYED_SIZE = 4 + static_cast<int>(sizeof(F1Udp::Car));

    WiFiUDP udp;
    uint16_t port;
    IPAddress relay, peerAddress, gameSender;
    uint32_t peerMs = 0;
    bool peerSeen = false;
    F1Udp::Decoder decoder;
    uint8_t buffer[F1Udp::MAX_PACKET];
    uint32_t lastSampleMs = 0;
    bool received = false;
    bool lastOnTrack = false, lastPaused = false;
    int32_t relayClock = 0;
    bool relayClockStarted = false;
    // Datagrams per second, the packet format of the last one.
    uint32_t lastDatagramMs = 0, windowStartMs = 0, windowCount = 0;
    float rate = 0;
    bool datagramsEver = false;
    uint16_t lastFormat = 0;

    bool accept(const F1Udp::Car &car)
    {
        lastSampleMs = millis();
        received = true;
        lastOnTrack = car.onTrack;
        lastPaused = car.paused;
        return true;
    }

    void countDatagram(int length)
    {
        const uint32_t now = millis();
        if (length > 0)
        {
            lastDatagramMs = now;
            datagramsEver = true;
            windowCount++;
        }
        if (now - windowStartMs >= 1000)
        {
            rate = windowCount * 1000.0f / (now - windowStartMs);
            windowCount = 0;
            windowStartMs = now;
        }
    }
};
