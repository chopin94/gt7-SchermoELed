#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <OtaWebUpdate.h>
#include "version.h"

// Password of the update page (user "admin"), empty for none: see the
// build_flags in platformio.ini.
#ifndef GT7_OTA_PASSWORD
#define GT7_OTA_PASSWORD ""
#endif

// Name checked by the update over Wi-Fi (lib/OtaUpdate/OtaImage.h): only a
// firmware.bin of the screen built for the same board is accepted.
static const char DASHBOARD_FIRMWARE_TAG[] = OTA_TAG_PREFIX "schermo-" GT7_DASH_BOARD;

// Update page of the screen at http://<address>/ while it is on the home
// Wi-Fi. It runs in the Arduino loop like the telemetry sockets: during an
// upload the loop waits for the whole file and the listener draws the
// progress. The setup portal of DashboardNetwork uses port 80 as well, but it
// opens only after the home network has been lost, so never at the same time.
class DashboardUpdate {
public:
    void begin(OtaWebUpdate::Listener listener, void *context) {
        server.enableDelay(false);
        server.on("/", HTTP_GET, [this]() {
            server.sendHeader("Location", "/update");
            server.send(302, "text/plain", "");
        });
        const OtaWebUpdate::Config config = {"GT7 Schermo", DASHBOARD_FIRMWARE_TAG,
                                             GT7_DASH_VERSION, GT7_OTA_PASSWORD};
        ota = OtaWebUpdate::attach(server, config, listener, context);
    }
    // online: connected to the home network, setup portal closed.
    void service(bool online) {
        if (online && !running) {
            server.begin();
            running = true;
            // A new firmware on trial got back on Wi-Fi with its update page:
            // it is good, so the bootloader stops falling back to the old one.
            OtaWebUpdate::confirmRunningFirmware();
        } else if (!online && running) {
            server.stop();
            running = false;
        }
        if (running) server.handleClient();
        if (ota) ota->loop();
    }
    bool ready() const { return running; }
private:
    OtaWebServer server{80};
    OtaWebUpdate *ota = nullptr;
    bool running = false;
};
