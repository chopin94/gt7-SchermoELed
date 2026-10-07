// Host stand-in for the WebServer of Arduino-ESP32 2.0.17, with the same
// handler interface: the test plays the role of the request parser.
#pragma once
#include <Arduino.h>
#include <vector>

enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };
enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
enum HTTPRawStatus { RAW_START, RAW_WRITE, RAW_END, RAW_ABORTED };
enum HTTPAuthMethod { BASIC_AUTH, DIGEST_AUTH };
#define HTTP_UPLOAD_BUFLEN 1436
#define HTTP_RAW_BUFLEN 1436

typedef struct {
  HTTPUploadStatus status;
  String  filename;
  String  name;
  String  type;
  size_t  totalSize;
  size_t  currentSize;
  uint8_t buf[HTTP_UPLOAD_BUFLEN];
} HTTPUpload;

typedef struct
{
  HTTPRawStatus status;
  size_t  totalSize;
  size_t  currentSize;
  uint8_t buf[HTTP_RAW_BUFLEN];
  void    *data;
} HTTPRaw;

class WebServer;

class RequestHandler {
public:
    virtual ~RequestHandler() { }
    virtual bool canHandle(HTTPMethod method, String uri) { (void) method; (void) uri; return false; }
    virtual bool canUpload(String uri) { (void) uri; return false; }
    virtual bool canRaw(String uri) { (void) uri; return false; }
    virtual bool handle(WebServer& server, HTTPMethod requestMethod, String requestUri) { (void) server; (void) requestMethod; (void) requestUri; return false; }
    virtual void upload(WebServer& server, String requestUri, HTTPUpload& upload) { (void) server; (void) requestUri; (void) upload; }
    virtual void raw(WebServer& server, String requestUri, HTTPRaw& raw) { (void) server; (void) requestUri; (void) raw; }
};

struct HostClient {
    uint32_t timeoutSeconds = 1;
    int setTimeout(uint32_t seconds) { timeoutSeconds = seconds; return 0; }
};

class WebServer {
public:
    explicit WebServer(int = 80) {}
    ~WebServer() { for (RequestHandler *h : handlers) delete h; }
    void addHandler(RequestHandler *handler) { handlers.push_back(handler); }
    bool authenticate(const char *, const char *password) { return credentials == password; }
    void requestAuthentication(HTTPAuthMethod = BASIC_AUTH, const char * = NULL, const String & = String(""))
    {
        code = 401;
    }
    int clientContentLength() { return contentLength; }
    void sendHeader(const String &, const String &, bool = false) {}
    void send(int status, const char *, const String &content) { code = status; body = content; }

    // Set by the test for the current request, read back after it.
    HostClient &client() { return _currentClient; }
    std::vector<RequestHandler *> handlers;
    String credentials;
    int contentLength = 0;
    int code = 0;
    String body;
protected:
    HostClient _currentClient;
};
