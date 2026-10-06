#pragma once
#include <WiFi.h>
class WiFiUDP {
public:
    uint8_t begin(uint16_t) { return 1; }
    void stop() {}
    int parsePacket() { return 0; }
    int read(void *, size_t) { return 0; }
    int read(char *, size_t) { return 0; }
    int read() { return -1; }
    int peek() { return -1; }
    void flush() {}
    int available() { return 0; }
    int beginPacket(IPAddress, uint16_t) { return 1; }
    int beginPacket(const char *, uint16_t) { return 1; }
    size_t write(const uint8_t *, size_t n) { return n; }
    size_t write(uint8_t) { return 1; }
    size_t print(const String &s) { return s.length(); }
    int endPacket() { return 1; }
    IPAddress remoteIP() const { return IPAddress(); }
    uint16_t remotePort() const { return 0; }
};
