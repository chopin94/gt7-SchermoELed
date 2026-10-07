// Checks made on a firmware received over Wi-Fi (lib/OtaUpdate/OtaImage.h).
#include "../lib/OtaUpdate/OtaImage.h"
#include <assert.h>
#include <algorithm>
#include <string>
#include <vector>

static const char OWN_TAG[] = OTA_TAG_PREFIX "schermo-esp32-2432s024c";

static std::vector<uint8_t> appHeader(uint16_t chip)
{
    std::vector<uint8_t> head(OtaImage::HEADER_BYTES, 0x11);
    head[0] = 0xE9;
    head[12] = uint8_t(chip);
    head[13] = uint8_t(chip >> 8);
    head[32] = 0x32;
    head[33] = 0x54;
    head[34] = 0xCD;
    head[35] = 0xAB;
    return head;
}

static std::vector<uint8_t> bytes(const std::string &text)
{
    return std::vector<uint8_t>(text.begin(), text.end());
}

// Some binary noise, the given content, more noise.
static std::vector<uint8_t> image(const std::string &content)
{
    std::vector<uint8_t> data;
    for (int i = 0; i < 300; ++i) data.push_back(uint8_t(i * 37 + 11));
    data.insert(data.end(), content.begin(), content.end());
    for (int i = 0; i < 300; ++i) data.push_back(uint8_t(i * 53 + 7));
    return data;
}

// Same verdict whatever the size of the network packets.
static void scan(const std::vector<uint8_t> &data, bool expectSame, const char *expectOther)
{
    for (size_t chunk = 1; chunk <= 64; ++chunk)
    {
        OtaImage::TagScanner scanner(OWN_TAG);
        for (size_t i = 0; i < data.size(); i += chunk)
            scanner.feed(data.data() + i, std::min(chunk, data.size() - i));
        assert(scanner.same() == expectSame);
        assert(std::string(scanner.otherName()) == expectOther);
        char reason[80];
        assert(scanner.verdict(reason, sizeof(reason)) == expectSame);
        if (!expectSame)
        {
            assert(reason[0] != 0);
            if (expectOther[0]) assert(std::string(reason).find(expectOther) != std::string::npos);
        }
    }
}

int main()
{
    using OtaImage::HeaderCheck;

    // Header: an application for the chip of this device.
    std::vector<uint8_t> head = appHeader(0);
    assert(OtaImage::checkHeader(head.data(), 0) == HeaderCheck::Ok);
    // LED strip firmware (ESP32-S2) sent to the screen (ESP32), and back.
    assert(OtaImage::checkHeader(appHeader(2).data(), 0) == HeaderCheck::OtherChip);
    assert(OtaImage::checkHeader(head.data(), 2) == HeaderCheck::OtherChip);
    char reason[80];
    OtaImage::describeHeader(HeaderCheck::OtherChip, appHeader(2).data(), 0, reason, sizeof(reason));
    assert(std::string(reason).find("ESP32-S2") != std::string::npos);
    // Not an ESP image (a zip, a text file).
    head[0] = 'P';
    assert(OtaImage::checkHeader(head.data(), 0) == HeaderCheck::NotFirmware);
    OtaImage::describeHeader(HeaderCheck::NotFirmware, head.data(), 0, reason, sizeof(reason));
    assert(std::string(reason).find("firmware.bin") != std::string::npos);
    // The bootloader (or a merged image starting with it) has no app descriptor.
    head = appHeader(0);
    head[33] = 0;
    assert(OtaImage::checkHeader(head.data(), 0) == HeaderCheck::NotApp);
    OtaImage::describeHeader(HeaderCheck::Ok, head.data(), 0, reason, sizeof(reason));
    assert(reason[0] == 0);
    assert(std::string(OtaImage::chipName(9)) == "ESP32-S3");

    // Tag of this board, also split across packets of any size.
    const std::string own = std::string(OWN_TAG) + '\0';
    scan(image(own), true, "");
    // The prefix alone (scanner code) and inside the page script are no tags.
    scan(image(std::string(OTA_TAG_PREFIX) + '\0' + "const TAG='GT7-FW:'+BOARD;"), false, "");
    scan(image(""), false, "");
    // Firmware of the LED strip or of another screen board.
    scan(image(std::string(OTA_TAG_PREFIX "led-lolin-s2-mini") + '\0'), false, "led-lolin-s2-mini");
    scan(image(std::string(OTA_TAG_PREFIX "schermo-esp32") + '\0'), false, "schermo-esp32");
    scan(image(std::string(OTA_TAG_PREFIX "schermo-esp32-2432s024cX") + '\0'), false,
         "schermo-esp32-2432s024cX");
    scan(image(std::string(OTA_TAG_PREFIX "SCHERMO-ESP32-2432S024C") + '\0'), false,
         "SCHERMO-ESP32-2432S024C");
    // Another tag before the right one: the right one wins.
    scan(image(std::string(OTA_TAG_PREFIX "led-lolin-s2-mini") + '\0' + own), true, "led-lolin-s2-mini");
    // A false start of the prefix right before the real tag.
    scan(image("G" + own), true, "");
    scan(image("GT7-" + own), true, "");
    scan(image("GT7-FW:x'" + own), true, "");
    // Names that never end, or end too late, are ignored.
    scan(image(std::string(OTA_TAG_PREFIX) + std::string(41, 'a') + '\0'), false, "");
    scan(image(std::string(OTA_TAG_PREFIX) + std::string(40, 'a') + '\0'), false, std::string(40, 'a').c_str());
    scan(image(std::string(OTA_TAG_PREFIX) + "schermo-esp32-2432s024c"), false, "");

    // reset() forgets what was found.
    OtaImage::TagScanner scanner(OWN_TAG);
    const std::vector<uint8_t> data = bytes(own);
    scanner.feed(data.data(), data.size());
    assert(scanner.same());
    scanner.reset();
    assert(!scanner.same() && scanner.otherName()[0] == 0);
    return 0;
}
