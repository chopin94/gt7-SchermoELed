#pragma once
#include <Arduino.h>
#include <map>
#include <string>
class Preferences {
public:
    bool begin(const char *, bool = false) { return true; }
    void end() {}
    bool clear() { values_.clear(); return true; }
    bool isKey(const char *k) { return values_.count(k) != 0; }
    uint8_t getUChar(const char *k, uint8_t d = 0) { auto i = values_.find(k); return i == values_.end() ? d : (uint8_t)i->second; }
    size_t putUChar(const char *k, uint8_t v) { values_[k] = v; return 1; }
    int32_t getInt(const char *k, int32_t d = 0) { auto i = values_.find(k); return i == values_.end() ? d : (int32_t)i->second; }
    size_t putInt(const char *k, int32_t v) { values_[k] = v; return 4; }
    uint32_t getUInt(const char *k, uint32_t d = 0) { auto i = values_.find(k); return i == values_.end() ? d : (uint32_t)i->second; }
    size_t putUInt(const char *k, uint32_t v) { values_[k] = v; return 4; }
    float getFloat(const char *k, float d = 0) { auto i = values_.find(k); return i == values_.end() ? d : (float)i->second; }
    size_t putFloat(const char *k, float v) { values_[k] = v; return 4; }
private:
    std::map<std::string, double> values_;
};
