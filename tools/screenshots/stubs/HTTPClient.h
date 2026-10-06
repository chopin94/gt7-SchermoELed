#pragma once
#include <Arduino.h>
class HTTPClient {
public:
    bool begin(const String &) { return true; }
    void setTimeout(uint16_t) {}
    int GET() { return -1; }
    String getString() { return String(); }
    void end() {}
};
