#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <sdkconfig.h>
#include <algorithm>
#include "OtaImage.h"

// Firmware update over Wi-Fi, shared by the screen and the LED strip.
//
// GET /update serves a page that checks the chosen file in the browser and
// sends it; POST /update takes the firmware.bin as the raw request body (the
// page, curl --data-binary, PlatformIO) or as a multipart form (curl -F).
// The image is written to the free OTA slot while it arrives and becomes the
// boot firmware only once it is complete and verified: an interrupted or
// refused transfer leaves the running firmware untouched.
//
// Include this header in one source file only. The WebServer owns the handler.

static const char OTA_PAGE[] PROGMEM = R"html(<!DOCTYPE html>
<html lang="it"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>%DEVICE% - Aggiornamento</title>
<style>
body{margin:0;font-family:system-ui,-apple-system,'Segoe UI',sans-serif;background:#0b0b10;color:#e8e8ee}
main{max-width:480px;margin:0 auto;padding:28px 18px}
h1{margin:0;font-size:1.5em}
.sub{color:#8a8a99;margin:6px 0 22px}
label{display:block;padding:26px 14px;border:2px dashed #3a3a48;border-radius:12px;text-align:center;cursor:pointer;color:#c8c8d4;line-height:1.6}
label b{color:#fff}
input{display:none}
button{width:100%;margin-top:14px;padding:15px;border:0;border-radius:12px;background:#e10600;color:#fff;font-size:1.05em;font-weight:600}
button:disabled{background:#2a2a33;color:#777}
.bar{height:10px;margin-top:16px;border-radius:5px;background:#22222b;overflow:hidden}
.bar div{height:100%;width:0;background:#e10600;transition:width .2s}
#msg{min-height:1.4em;margin:14px 0 0;line-height:1.45}
.ok{color:#36d36b}.err{color:#ff5a5a}
.note{color:#8a8a99;font-size:.9em;line-height:1.45;margin-top:26px}
</style></head><body><main>
<h1>%DEVICE%</h1>
<p class="sub">Versione %VERSION% &middot; %BOARD%</p>
<label>Scegli il file <b>firmware.bin</b><br><span id="name">nessun file</span><input id="file" type="file" accept=".bin"></label>
<button id="send" disabled>Aggiorna</button>
<div class="bar"><div id="fill"></div></div>
<p id="msg"></p>
<p class="note">Usa il firmware.bin compilato per questa scheda, non bootloader.bin o partitions.bin.
Se il trasferimento non va a buon fine il firmware attuale resta com'&egrave;; se quello nuovo non riparte
o non torna sul Wi-Fi, al riavvio successivo il dispositivo rimette da solo la versione precedente.</p>
</main><script>
var MAX=%MAX%,CHIP=%CHIP%,BOARD='%BOARD%',ok=false;
function $(id){return document.getElementById(id)}
function show(t,c){$('msg').textContent=t;$('msg').className=c||''}
function kb(n){return Math.round(n/1024)+' KB'}
// Same checks as the device (OtaImage.h), before sending anything.
function check(u){
 if(u.length<64||u[0]!=0xE9)return 'Non è un firmware: scegli il file firmware.bin';
 if((u[12]|u[13]<<8)!=CHIP)return 'Firmware per un altro chip: è quello di un altro dispositivo';
 if(u[32]!=0x32||u[33]!=0x54||u[34]!=0xCD||u[35]!=0xAB)return 'Serve firmware.bin, non bootloader.bin o l\'immagine completa';
 if(u.length>MAX)return 'Firmware troppo grande: '+kb(u.length)+', lo spazio è '+kb(MAX);
 var p=[71,84,55,45,70,87,58],other='';
 for(var i=u.indexOf(71);i>=0;i=u.indexOf(71,i+1)){
  var j=0;while(j<7&&u[i+j]==p[j])j++;
  if(j<7)continue;
  var k=i+7,n='';
  while(k<u.length&&n.length<=40&&/[A-Za-z0-9_.-]/.test(String.fromCharCode(u[k])))n+=String.fromCharCode(u[k++]);
  if(!n.length||n.length>40||u[k]!==0)continue;
  if(n==BOARD)return '';
  if(!other)other=n;
 }
 return other?'Firmware per un\'altra scheda: '+other:'Firmware senza aggiornamento Wi-Fi: caricalo via USB';
}
$('file').onchange=function(){
 var f=this.files[0];ok=false;$('send').disabled=true;$('fill').style.width='0';show('');
 if(!f)return;
 $('name').textContent=f.name+' ('+kb(f.size)+')';
 var r=new FileReader();
 r.onload=function(){var e=check(new Uint8Array(r.result));
  if(e)show(e,'err');else{ok=true;$('send').disabled=false;show('File valido: premi Aggiorna')}};
 r.onerror=function(){show('Impossibile leggere il file','err')};
 r.readAsArrayBuffer(f);
};
$('send').onclick=function(){
 var f=$('file').files[0];if(!ok||!f)return;
 $('send').disabled=true;$('file').disabled=true;show('Invio...');
 var x=new XMLHttpRequest();
 x.open('POST','/update');
 x.setRequestHeader('Content-Type','application/octet-stream');
 x.upload.onprogress=function(e){if(!e.lengthComputable)return;var p=Math.round(100*e.loaded/e.total);
  $('fill').style.width=p+'%';show(p<100?'Invio '+p+'%':'Verifica e installazione...')};
 x.onload=function(){$('file').disabled=false;
  if(x.status==200){$('fill').style.width='100%';$('fill').style.background='#36d36b';
   show(x.responseText+'. Tra una decina di secondi ricarica la pagina per controllare la versione.','ok')}
  else{show(x.responseText||('Errore '+x.status),'err');$('send').disabled=false}};
 x.onerror=function(){$('file').disabled=false;$('send').disabled=false;
  show('Connessione interrotta: il firmware attuale non è cambiato, riprova','err')};
 x.send(f);
};
</script></body></html>
)html";

// The stock WebServer gives up a request after one second without data: too
// little for a 2 MB file over Wi-Fi, where a lost packet can stall the
// transfer for longer. While a file arrives this one waits a few seconds.
class OtaWebServer : public WebServer
{
public:
    explicit OtaWebServer(int port = 80) : WebServer(port) {}
    void waitForSlowClient() { _currentClient.setTimeout(5); }
};

class OtaWebUpdate : public RequestHandler
{
public:
    struct Config
    {
        const char *device;   // name on the page: "GT7 Schermo"
        const char *tag;      // OTA_TAG_PREFIX "<name>" of this firmware
        const char *version;
        const char *password; // user "admin"; nullptr or "" for no password
    };
    typedef void (*Listener)(const OtaImage::Progress &progress, void *context);

    static OtaWebUpdate *attach(OtaWebServer &server, const Config &config,
                                Listener listener = nullptr, void *context = nullptr)
    {
        OtaWebUpdate *handler = new OtaWebUpdate(server, config, listener, context);
        server.addHandler(handler);
        return handler;
    }

    // Call from the loop: restarts into the new firmware once the reply has
    // had time to reach the browser.
    void loop()
    {
        if (restartPending && int32_t(millis() - restartAt) >= 0) ESP.restart();
    }

    const OtaImage::Progress &progress() const { return state; }

    // After an update the bootloader starts the new firmware on trial: if the
    // device restarts before the firmware is confirmed, the previous one comes
    // back. Needs verifyRollbackLater() returning true; call this once the new
    // firmware has shown it works, when it is back on Wi-Fi with this page
    // available, so a further update can always reach the device.
    static void confirmRunningFirmware()
    {
        const esp_partition_t *running = esp_ota_get_running_partition();
        esp_ota_img_states_t imageState;
        if (running && esp_ota_get_state_partition(running, &imageState) == ESP_OK &&
            imageState == ESP_OTA_IMG_PENDING_VERIFY)
            esp_ota_mark_app_valid_cancel_rollback();
    }

    // Room for the next firmware, 0 if the partition table has no OTA slot.
    static uint32_t slotSize()
    {
        const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
        return next ? next->size : 0;
    }

private:
    static constexpr uint32_t NOTIFY_BYTES = 16384;
    static constexpr uint32_t RESTART_DELAY_MS = 1500;

    OtaWebServer &owner;
    const Config config;
    const Listener listener;
    void *const context;
    OtaImage::Progress state;
    OtaImage::TagScanner scanner;
    uint8_t head[OtaImage::HEADER_BYTES];
    size_t headLength = 0;
    uint32_t declaredSize = 0; // exact size of a raw body, 0 for forms
    uint32_t notifiedBytes = 0;
    bool fileSeen = false;     // the current request carried a file
    bool ignoredForRestart = false;
    bool authorized = true;
    bool writing = false;      // Update.begin() done
    bool restartPending = false;
    uint32_t restartAt = 0;

    OtaWebUpdate(OtaWebServer &owner, const Config &config, Listener listener, void *context)
        : owner(owner), config(config), listener(listener), context(context), scanner(config.tag) {}

    bool passwordRequired() const { return config.password && config.password[0]; }

    bool canHandle(HTTPMethod method, String uri) override
    {
        return uri == "/update" && (method == HTTP_GET || method == HTTP_POST);
    }
    bool canUpload(String uri) override { return uri == "/update"; }
    bool canRaw(String uri) override { return uri == "/update"; }

    // Called after the body has been read through upload() or raw().
    bool handle(WebServer &server, HTTPMethod method, String) override
    {
        const bool sentFile = fileSeen;
        const bool busy = ignoredForRestart;
        fileSeen = ignoredForRestart = false;
        // For an upload the password was checked when the file started.
        const bool allowed = method == HTTP_POST && sentFile
            ? authorized
            : !passwordRequired() || server.authenticate("admin", config.password);
        if (!allowed)
        {
            server.requestAuthentication(BASIC_AUTH, config.device, "Password richiesta");
            return true;
        }
        if (method == HTTP_GET)
        {
            sendPage(server);
            return true;
        }
        server.sendHeader("Connection", "close");
        if (busy)
            server.send(503, "text/plain; charset=utf-8", "Riavvio in corso: riprova tra poco");
        else if (!sentFile)
            server.send(400, "text/plain; charset=utf-8", "Nessun firmware ricevuto");
        else
            server.send(state.phase == OtaImage::Phase::Done ? 200 : 400,
                        "text/plain; charset=utf-8", state.message);
        return true;
    }

    void upload(WebServer &server, String, HTTPUpload &file) override
    {
        switch (file.status)
        {
        case UPLOAD_FILE_START: start(server, 0); break;
        case UPLOAD_FILE_WRITE: write(file.buf, file.currentSize); break;
        case UPLOAD_FILE_END: finish(); break;
        case UPLOAD_FILE_ABORTED: interrupted(); break;
        }
    }

    void raw(WebServer &server, String, HTTPRaw &body) override
    {
        switch (body.status)
        {
        case RAW_START: start(server, uint32_t(std::max(server.clientContentLength(), 0))); break;
        case RAW_WRITE: write(body.buf, body.currentSize); break;
        case RAW_END: finish(); break;
        case RAW_ABORTED: interrupted(); break;
        }
    }

    void sendPage(WebServer &server)
    {
        String page(OTA_PAGE);
        page.replace("%DEVICE%", config.device);
        page.replace("%VERSION%", config.version);
        page.replace("%BOARD%", config.tag + sizeof(OTA_TAG_PREFIX) - 1);
        page.replace("%CHIP%", String(CONFIG_IDF_FIRMWARE_CHIP_ID));
        page.replace("%MAX%", String(slotSize()));
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "text/html; charset=utf-8", page);
    }

    void start(WebServer &server, uint32_t rawSize)
    {
        fileSeen = true;
        owner.waitForSlowClient();
        // The new firmware is already in place: never touch it again.
        if (restartPending)
        {
            ignoredForRestart = true;
            return;
        }
        if (Update.isRunning()) Update.abort(); // left over by a broken request
        writing = false;
        headLength = 0;
        notifiedBytes = 0;
        declaredSize = rawSize;
        scanner.reset();
        state = OtaImage::Progress();
        state.phase = OtaImage::Phase::Receiving;
        // For forms the length includes the boundaries: fine for a progress bar.
        state.total = rawSize ? rawSize : uint32_t(std::max(server.clientContentLength(), 0));
        authorized = !passwordRequired() || server.authenticate("admin", config.password);
        const uint32_t slot = slotSize();
        if (!authorized)
            fail("Password errata");
        else if (!slot)
            fail("Partizioni senza OTA: carica questo firmware via USB");
        else if (declaredSize > slot)
        {
            snprintf(state.message, sizeof(state.message), "Firmware troppo grande: %u KB, lo spazio è %u KB",
                     unsigned(declaredSize / 1024), unsigned(slot / 1024));
            fail(nullptr);
        }
        else
            notify();
    }

    void write(const uint8_t *data, size_t length)
    {
        if (state.phase != OtaImage::Phase::Receiving || length == 0) return;
        state.received += length;
        if (!writing)
        {
            // The checks need the first HEADER_BYTES of the image.
            const size_t take = std::min(length, sizeof(head) - headLength);
            memcpy(head + headLength, data, take);
            headLength += take;
            data += take;
            length -= take;
            if (headLength < sizeof(head) || !begin()) return;
        }
        if (length && !store(data, length)) return;
        if (state.received - notifiedBytes >= NOTIFY_BYTES) notify();
    }

    bool begin()
    {
        const OtaImage::HeaderCheck check = OtaImage::checkHeader(head, CONFIG_IDF_FIRMWARE_CHIP_ID);
        if (check != OtaImage::HeaderCheck::Ok)
        {
            OtaImage::describeHeader(check, head, CONFIG_IDF_FIRMWARE_CHIP_ID, state.message, sizeof(state.message));
            fail(nullptr);
            return false;
        }
        if (!Update.begin(declaredSize ? declaredSize : UPDATE_SIZE_UNKNOWN))
        {
            failUpdate();
            return false;
        }
        writing = true;
        return store(head, headLength);
    }

    bool store(const uint8_t *data, size_t length)
    {
        scanner.feed(data, length);
        if (Update.write(const_cast<uint8_t *>(data), length) == length) return true;
        failUpdate();
        return false;
    }

    void finish()
    {
        if (state.phase != OtaImage::Phase::Receiving) return;
        if (!writing)
        {
            fail(state.received ? "File troppo corto: non è un firmware" : "Nessun dato ricevuto");
            return;
        }
        if (!scanner.verdict(state.message, sizeof(state.message)))
        {
            fail(nullptr);
            return;
        }
        // end() checks the image (checksum and SHA-256) before making it the
        // boot firmware.
        if (!Update.end(declaredSize == 0))
        {
            failUpdate();
            return;
        }
        writing = false;
        state.phase = OtaImage::Phase::Done;
        snprintf(state.message, sizeof(state.message), "Aggiornato: riavvio con la versione nuova");
        // Restart even if the reply never leaves (client already gone).
        restartPending = true;
        restartAt = millis() + RESTART_DELAY_MS;
        notify();
    }

    // The server drops the request afterwards, without calling handle().
    void interrupted()
    {
        fileSeen = ignoredForRestart = false;
        if (state.phase == OtaImage::Phase::Receiving)
            fail("Trasferimento interrotto: il firmware attuale non è cambiato");
    }

    void fail(const char *message)
    {
        if (writing) Update.abort();
        writing = false;
        if (message) snprintf(state.message, sizeof(state.message), "%s", message);
        state.phase = OtaImage::Phase::Failed;
        notify();
    }

    void failUpdate()
    {
        const char *reason;
        switch (Update.getError())
        {
        case UPDATE_ERROR_OK: reason = "memoria insufficiente"; break;
        case UPDATE_ERROR_WRITE:
        case UPDATE_ERROR_ERASE: reason = "scrittura nella flash non riuscita"; break;
        case UPDATE_ERROR_SPACE:
        case UPDATE_ERROR_SIZE: reason = "il firmware non entra nello spazio libero"; break;
        case UPDATE_ERROR_READ:
        case UPDATE_ERROR_ACTIVATE: reason = "immagine danneggiata o incompleta"; break;
        case UPDATE_ERROR_NO_PARTITION: reason = "nessuno slot per l'aggiornamento"; break;
        default: reason = Update.errorString(); break;
        }
        snprintf(state.message, sizeof(state.message), "Aggiornamento non riuscito: %s", reason);
        fail(nullptr);
    }

    void notify()
    {
        notifiedBytes = state.received;
        if (listener) listener(state, context);
        // Lets the idle task run: single-core chips stay in this request for
        // the whole transfer.
        delay(1);
    }
};
