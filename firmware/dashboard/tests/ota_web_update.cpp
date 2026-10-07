// Firmware update over Wi-Fi (lib/OtaUpdate/OtaWebUpdate.h) driven like the
// WebServer of Arduino-ESP32 does: raw bodies, multipart forms, aborted
// transfers, password, restart. tests/ota_stubs stands in for the ESP32 side.
#include <OtaWebUpdate.h>
#include <assert.h>
#include <string>
#include <vector>

uint32_t g_hostMillis = 1000;
UpdateClass Update;
HostEsp ESP;
esp_partition_t g_running = {0x10000, 0x1F0000, "app0"};
esp_partition_t g_next = {0x200000, 0x1F0000, "app1"};
esp_partition_t *g_runningPartition = &g_running;
esp_partition_t *g_nextPartition = &g_next;
esp_ota_img_states_t g_runningState = ESP_OTA_IMG_VALID;
int g_markedValid = 0;

static const char TAG[] = OTA_TAG_PREFIX "schermo-esp32-2432s024c";
static std::vector<OtaImage::Phase> g_phases;
static std::string g_lastMessage;

static void listen(const OtaImage::Progress &progress, void *context)
{
    assert(context == &g_phases);
    if (g_phases.empty() || g_phases.back() != progress.phase) g_phases.push_back(progress.phase);
    g_lastMessage = progress.message;
}

// An application image: ESP header, app descriptor, noise and the tag.
static std::vector<uint8_t> firmware(size_t size, const std::string &tag, uint16_t chip = 0, bool app = true)
{
    std::vector<uint8_t> image(size);
    for (size_t i = 0; i < size; ++i) image[i] = uint8_t(i * 131 + 7);
    image[0] = 0xE9;
    image[12] = uint8_t(chip);
    image[13] = uint8_t(chip >> 8);
    const uint8_t desc[4] = {0x32, 0x54, 0xCD, 0xAB};
    memcpy(&image[32], desc, 4);
    if (!app) image[33] = 0;
    if (!tag.empty()) memcpy(&image[size / 2], tag.c_str(), tag.size() + 1);
    return image;
}

struct Fixture
{
    OtaWebServer server;
    RequestHandler *handler;
    OtaWebUpdate *ota;

    explicit Fixture(const char *password = nullptr)
    {
        Update = UpdateClass();
        ESP = HostEsp();
        g_phases.clear();
        const OtaWebUpdate::Config config = {"GT7 Schermo", TAG, "2.1.0", password};
        ota = OtaWebUpdate::attach(server, config, listen, &g_phases);
        handler = server.handlers.at(0);
    }

    // POST /update with the file as the raw body, in packets of `chunk` bytes.
    void postRaw(const std::vector<uint8_t> &body, size_t chunk = HTTP_RAW_BUFLEN, size_t abortAt = 0)
    {
        server.code = 0;
        server.contentLength = int(body.size());
        assert(handler->canHandle(HTTP_POST, "/update") && handler->canRaw("/update"));
        HTTPRaw raw = {};
        raw.status = RAW_START;
        server.client().timeoutSeconds = 1;
        handler->raw(server, "/update", raw);
        // Waits for a slow network while the file arrives.
        assert(server.client().timeoutSeconds == 5);
        raw.status = RAW_WRITE;
        for (size_t i = 0; i < body.size(); i += chunk)
        {
            if (abortAt && i >= abortAt)
            {
                raw.status = RAW_ABORTED;
                handler->raw(server, "/update", raw);
                return; // the real server drops the request: no handle()
            }
            raw.currentSize = std::min(chunk, body.size() - i);
            memcpy(raw.buf, &body[i], raw.currentSize);
            handler->raw(server, "/update", raw);
        }
        raw.status = RAW_END;
        handler->raw(server, "/update", raw);
        handler->handle(server, HTTP_POST, "/update");
    }

    // POST /update as a multipart form (curl -F firmware=@firmware.bin).
    void postForm(const std::vector<uint8_t> &file)
    {
        server.code = 0;
        server.contentLength = int(file.size() + 190);
        assert(handler->canUpload("/update"));
        HTTPUpload upload;
        upload.status = UPLOAD_FILE_START;
        upload.currentSize = 0;
        handler->upload(server, "/update", upload);
        upload.status = UPLOAD_FILE_WRITE;
        for (size_t i = 0; i < file.size(); i += HTTP_UPLOAD_BUFLEN)
        {
            upload.currentSize = std::min<size_t>(HTTP_UPLOAD_BUFLEN, file.size() - i);
            memcpy(upload.buf, &file[i], upload.currentSize);
            handler->upload(server, "/update", upload);
        }
        upload.status = UPLOAD_FILE_END;
        handler->upload(server, "/update", upload);
        handler->handle(server, HTTP_POST, "/update");
    }

    std::string body() const { return std::string(server.body.c_str()); }
};

static bool contains(const std::string &text, const char *part) { return text.find(part) != std::string::npos; }

int main()
{
    using OtaImage::Phase;
    const std::string own = TAG;

    // The page shows version and board and carries the limits for the browser.
    {
        Fixture f;
        assert(f.handler->canHandle(HTTP_GET, "/update") && !f.handler->canHandle(HTTP_GET, "/"));
        f.handler->handle(f.server, HTTP_GET, "/update");
        assert(f.server.code == 200);
        assert(contains(f.body(), "Versione 2.1.0") && contains(f.body(), "schermo-esp32-2432s024c"));
        assert(contains(f.body(), "MAX=2031616,CHIP=0,BOARD='schermo-esp32-2432s024c'"));
        assert(!contains(f.body(), "%"
                         "MAX%"));
    }

    // A good firmware as raw body: written, verified, restart after the reply.
    {
        Fixture f;
        const std::vector<uint8_t> image = firmware(200000, own);
        f.postRaw(image);
        assert(Update.bootSet && !Update.endedEvenIfRemaining && Update.image == image);
        assert(f.server.code == 200 && contains(f.body(), "Aggiornato"));
        assert(f.ota->progress().phase == Phase::Done && f.ota->progress().received == image.size());
        assert(f.ota->progress().total == image.size());
        assert(g_phases.size() == 2 && g_phases[0] == Phase::Receiving && g_phases[1] == Phase::Done);
        f.ota->loop();
        assert(ESP.restarts == 0);
        g_hostMillis += 1600;
        f.ota->loop();
        assert(ESP.restarts == 1);
        // Another upload before the restart is refused and changes nothing.
        f.postRaw(firmware(1000, own));
        assert(f.server.code == 503 && Update.begins == 1 && f.ota->progress().phase == Phase::Done);
    }

    // Tiny packets: the header and the tag arrive split.
    {
        Fixture f;
        f.postRaw(firmware(5000, own), 7);
        assert(f.server.code == 200 && Update.bootSet);
    }

    // Multipart form: size unknown in advance.
    {
        Fixture f;
        const std::vector<uint8_t> image = firmware(100000, own);
        f.postForm(image);
        assert(f.server.code == 200 && Update.bootSet && Update.endedEvenIfRemaining && Update.image == image);
        assert(f.ota->progress().total == image.size() + 190);
    }

    // Refused files: the update never starts or is aborted, nothing becomes bootable.
    struct Refused
    {
        std::vector<uint8_t> image;
        const char *reason;
        bool started;
    };
    std::vector<uint8_t> zip(5000, 'x');
    zip[0] = 'P';
    const Refused refused[] = {
        {firmware(5000, own, 2), "ESP32-S2", false},                         // LED strip firmware
        {firmware(5000, own, 0, false), "bootloader.bin", false},            // bootloader
        {zip, "firmware.bin", false},                                        // not an image
        {firmware(5000, ""), "senza aggiornamento", true},                   // old firmware
        {firmware(5000, OTA_TAG_PREFIX "schermo-esp32-st7789"), "schermo-esp32-st7789", true},
        {std::vector<uint8_t>(20, 0xE9), "troppo corto", false},
    };
    for (const Refused &r : refused)
    {
        Fixture f;
        f.postRaw(r.image);
        assert(f.server.code == 400 && contains(f.body(), r.reason));
        assert(f.ota->progress().phase == Phase::Failed && g_phases.back() == Phase::Failed);
        assert(!Update.bootSet && !Update.running && Update.begins == (r.started ? 1 : 0));
        assert(contains(g_lastMessage, r.reason));
        g_hostMillis += 5000;
        f.ota->loop();
        assert(ESP.restarts == 0);
    }

    // Larger than the free slot: refused before writing anything.
    {
        Fixture f;
        Update.partitionSize = 4096;
        g_next.size = 4096;
        f.postRaw(firmware(5000, own));
        assert(f.server.code == 400 && contains(f.body(), "troppo grande") && Update.begins == 0);
        g_next.size = 0x1F0000;
    }

    // Partition table without OTA (huge_app.csv): only a USB upload helps.
    {
        Fixture f;
        g_nextPartition = nullptr;
        f.postRaw(firmware(5000, own));
        assert(f.server.code == 400 && contains(f.body(), "via USB") && Update.begins == 0);
        g_nextPartition = &g_next;
    }

    // Connection lost halfway: the running firmware stays.
    {
        Fixture f;
        f.postRaw(firmware(50000, own), HTTP_RAW_BUFLEN, 20000);
        assert(f.ota->progress().phase == Phase::Failed && contains(g_lastMessage, "interrotto"));
        assert(!Update.running && !Update.bootSet && Update.aborts == 1);
        // The next attempt works.
        f.postRaw(firmware(50000, own));
        assert(f.server.code == 200 && Update.bootSet);
    }

    // Flash write error and an image that fails verification.
    {
        Fixture f;
        Update.failWriteAt = 30000;
        f.postRaw(firmware(50000, own));
        assert(f.server.code == 400 && contains(f.body(), "scrittura"));
    }
    {
        Fixture f;
        Update.failVerify = true;
        f.postRaw(firmware(50000, own));
        assert(f.server.code == 400 && contains(f.body(), "danneggiata") && !Update.bootSet);
    }

    // Empty body, and a form without a file.
    {
        Fixture f;
        f.postRaw(std::vector<uint8_t>());
        assert(f.server.code == 400 && contains(f.body(), "Nessun dato"));
        f.handler->handle(f.server, HTTP_POST, "/update");
        assert(f.server.code == 400 && contains(f.body(), "Nessun firmware"));
    }

    // Password: page and upload need it.
    {
        Fixture f("segreta");
        f.handler->handle(f.server, HTTP_GET, "/update");
        assert(f.server.code == 401);
        f.postRaw(firmware(5000, own));
        assert(f.server.code == 401 && Update.begins == 0 && contains(g_lastMessage, "Password"));
        f.server.credentials = "segreta";
        f.handler->handle(f.server, HTTP_GET, "/update");
        assert(f.server.code == 200);
        f.postRaw(firmware(5000, own));
        assert(f.server.code == 200 && Update.bootSet);
    }

    // Rollback: only a firmware still on trial gets confirmed.
    OtaWebUpdate::confirmRunningFirmware();
    assert(g_markedValid == 0);
    g_runningState = ESP_OTA_IMG_PENDING_VERIFY;
    OtaWebUpdate::confirmRunningFirmware();
    assert(g_markedValid == 1 && g_runningState == ESP_OTA_IMG_VALID);
    OtaWebUpdate::confirmRunningFirmware();
    assert(g_markedValid == 1);
    return 0;
}
