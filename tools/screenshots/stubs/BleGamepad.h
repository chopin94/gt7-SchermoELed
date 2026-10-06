#pragma once
// Bluetooth gamepad is commented out in the firmware; only the objects exist.
class BleGamepadConfiguration {};
class BleGamepad {
public:
    BleGamepad(const char * = "", const char * = "", int = 100) {}
    void begin(BleGamepadConfiguration * = nullptr) {}
    bool isConnected() { return false; }
};
