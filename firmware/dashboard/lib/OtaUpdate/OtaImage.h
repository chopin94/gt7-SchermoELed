#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Checks on a firmware image that arrives over Wi-Fi, made before it may
// replace the running firmware. Plain C++ without Arduino types, so the same
// code runs on the ESP32 and in the unit tests (tests/ota_image.cpp).

// Every firmware of the project contains the C string OTA_TAG_PREFIX "<name>",
// for instance "GT7-FW:schermo-esp32-2432s024c". An update is accepted only
// when it carries the same name as the running firmware: same device and same
// PlatformIO board. Older firmware without the tag cannot be updated over
// Wi-Fi again, so it is refused as well.
#define OTA_TAG_PREFIX "GT7-FW:"

namespace OtaImage
{
enum class Phase : uint8_t
{
    Idle,
    Receiving,
    Done,   // written and verified: the device restarts into it
    Failed, // nothing changed, the running firmware stays
};

// State of the transfer, drawn by the screen and by the LED strip.
struct Progress
{
    Phase phase = Phase::Idle;
    uint32_t received = 0;
    uint32_t total = 0; // 0 when the size is not known in advance
    char message[80] = {};
};

// esp_image_header_t (24 bytes) and the header of the first segment (8 bytes),
// then the esp_app_desc_t that every ESP-IDF application starts with.
static constexpr size_t HEADER_BYTES = 36;
static constexpr uint8_t IMAGE_MAGIC = 0xE9;
static constexpr uint32_t APP_DESC_MAGIC = 0xABCD5432;

enum class HeaderCheck : uint8_t
{
    Ok,
    NotFirmware, // not an ESP image at all
    OtherChip,   // image for another ESP32 variant (screen vs LED strip)
    NotApp,      // bootloader, or a merged image with the bootloader in front
};

// esp_chip_id_t of the image.
inline uint16_t imageChip(const uint8_t *head)
{
    return uint16_t(head[12] | head[13] << 8);
}

inline HeaderCheck checkHeader(const uint8_t *head, uint16_t chipId)
{
    if (head[0] != IMAGE_MAGIC) return HeaderCheck::NotFirmware;
    if (imageChip(head) != chipId) return HeaderCheck::OtherChip;
    const uint32_t magic = uint32_t(head[32]) | uint32_t(head[33]) << 8 |
        uint32_t(head[34]) << 16 | uint32_t(head[35]) << 24;
    return magic == APP_DESC_MAGIC ? HeaderCheck::Ok : HeaderCheck::NotApp;
}

inline const char *chipName(uint16_t chip)
{
    switch (chip)
    {
    case 0: return "ESP32";
    case 2: return "ESP32-S2";
    case 5: return "ESP32-C3";
    case 9: return "ESP32-S3";
    case 12: return "ESP32-C2";
    case 13: return "ESP32-C6";
    case 16: return "ESP32-H2";
    default: return "un altro chip";
    }
}

// Reason for a refused header, as shown to the user.
inline void describeHeader(HeaderCheck check, const uint8_t *head, uint16_t chipId,
                           char *out, size_t size)
{
    switch (check)
    {
    case HeaderCheck::NotFirmware:
        snprintf(out, size, "Non è un firmware: scegli il file firmware.bin");
        break;
    case HeaderCheck::OtherChip:
        snprintf(out, size, "Firmware per %s, questo dispositivo è %s",
                 chipName(imageChip(head)), chipName(chipId));
        break;
    case HeaderCheck::NotApp:
        snprintf(out, size, "Serve firmware.bin, non bootloader.bin o l'immagine completa");
        break;
    default:
        if (size) out[0] = 0;
        break;
    }
}

// Looks for the OTA_TAG_PREFIX "<name>" strings in an image fed chunk by chunk,
// so a tag split between two network packets is still found.
class TagScanner
{
public:
    static constexpr size_t MAX_NAME_LENGTH = 40;

    // ownTag: the tag of the running firmware, OTA_TAG_PREFIX "<name>".
    explicit TagScanner(const char *ownTag) : own(ownTag) { reset(); }

    void reset()
    {
        matched = 0;
        nameLength = 0;
        inName = false;
        sameFound = false;
        other[0] = 0;
    }

    void feed(const uint8_t *data, size_t length)
    {
        for (size_t i = 0; i < length; ++i) step(data[i]);
    }

    // The image carries the same name as the running firmware.
    bool same() const { return sameFound; }
    // First name of another device or board found, "" if none.
    const char *otherName() const { return other; }

    // After the whole image: false with the reason when the tag does not match.
    bool verdict(char *out, size_t size) const
    {
        if (sameFound) return true;
        if (other[0])
            snprintf(out, size, "Firmware per un'altra scheda: %s", other);
        else
            snprintf(out, size, "Firmware senza aggiornamento Wi-Fi: caricalo via USB");
        return false;
    }

private:
    static constexpr size_t PREFIX_LENGTH = sizeof(OTA_TAG_PREFIX) - 1;

    const char *own;
    size_t matched;
    size_t nameLength;
    bool inName;
    bool sameFound;
    char name[MAX_NAME_LENGTH + 1];
    char other[MAX_NAME_LENGTH + 1];

    static bool nameChar(uint8_t c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.';
    }

    void step(uint8_t c)
    {
        if (inName)
        {
            if (c == 0 && nameLength > 0)
            {
                name[nameLength] = 0;
                found();
                inName = false;
                matched = 0;
                return;
            }
            if (nameChar(c) && nameLength < MAX_NAME_LENGTH)
            {
                name[nameLength++] = char(c);
                return;
            }
            // Not a tag (the prefix alone, or inside other text): this byte
            // may still start the next prefix.
            inName = false;
            matched = 0;
        }
        // The prefix starts with the only 'G' it contains, so on a mismatch
        // the match can restart only from this byte.
        if (c == uint8_t(own[matched]))
        {
            if (++matched == PREFIX_LENGTH)
            {
                inName = true;
                nameLength = 0;
            }
        }
        else
            matched = c == uint8_t(own[0]) ? 1 : 0;
    }

    void found()
    {
        if (strcmp(name, own + PREFIX_LENGTH) == 0)
            sameFound = true;
        else if (!other[0])
            memcpy(other, name, nameLength + 1);
    }
};
} // namespace OtaImage
