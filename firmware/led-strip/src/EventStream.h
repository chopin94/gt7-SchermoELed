#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <lwip/sockets.h>

// Live telemetry for the web page as Server-Sent Events (GET /api/events):
// the browser keeps the connection open and reconnects by itself.
//
// The stock WebServer answers one request at a time and drops its copy of
// the connection afterwards; a copy kept here keeps the socket open. Frames
// are sent without ever waiting: a browser that cannot keep up (a phone in
// the pocket, weak Wi-Fi) skips frames, and after 5 s it is closed, so the
// LEDs never stall on the network.
class EventStream
{
public:
    static constexpr int MAX_CLIENTS = 4;

    // From the handler of /api/events: takes over the connection.
    void accept(WebServer &server)
    {
        WiFiClient client = server.client();
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS && slot < 0; ++i)
            if (!used(i)) slot = i;
        if (slot < 0)
        {
            // All taken: the oldest makes room.
            slot = 0;
            for (int i = 1; i < MAX_CLIENTS; ++i)
                if (since[i] < since[slot]) slot = i;
            clients[slot].stop();
        }
        client.setNoDelay(true);
        static const char head[] =
            "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-store\r\n"
            "Connection: keep-alive\r\nAccess-Control-Allow-Origin: *\r\n\r\nretry: 1500\n\n";
        client.write(reinterpret_cast<const uint8_t *>(head), sizeof(head) - 1);
        clients[slot] = client;
        since[slot] = millis();
        stalledSince[slot] = 0;
    }

    int count()
    {
        int n = 0;
        for (int i = 0; i < MAX_CLIENTS; ++i) n += used(i) ? 1 : 0;
        return n;
    }

    // "event: <name>\ndata: <data>\n\n" to every client that can take it now.
    void send(const char *name, const char *data, size_t length)
    {
        char head[32];
        const int headLength = snprintf(head, sizeof(head), "event: %s\ndata: ", name);
        if (headLength <= 0 || headLength >= static_cast<int>(sizeof(head)) || length + headLength + 2 > sizeof(message)) return;
        memcpy(message, head, headLength);
        memcpy(message + headLength, data, length);
        message[headLength + length] = '\n';
        message[headLength + length + 1] = '\n';
        const size_t total = headLength + length + 2;
        const uint32_t now = millis();
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (!used(i)) continue;
            const int fd = clients[i].fd();
            // Writable means more than half of the send buffer free (lwIP),
            // far more than a frame: then the whole frame goes at once.
            fd_set set;
            FD_ZERO(&set);
            FD_SET(fd, &set);
            struct timeval tv = {0, 0};
            const int ready = select(fd + 1, nullptr, &set, nullptr, &tv);
            if (ready < 0) { drop(i); continue; }
            if (ready == 0 || !FD_ISSET(fd, &set))
            {
                if (!stalledSince[i]) stalledSince[i] = now | 1;
                else if (now - stalledSince[i] > 5000) drop(i);
                continue;
            }
            const ssize_t sent = lwip_send(fd, message, total, MSG_DONTWAIT);
            if (sent == static_cast<ssize_t>(total)) { stalledSince[i] = 0; continue; }
            if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                if (!stalledSince[i]) stalledSince[i] = now | 1;
                continue;
            }
            // Closed by the browser, or half a frame: start over.
            drop(i);
        }
    }

private:
    WiFiClient clients[MAX_CLIENTS];
    uint32_t since[MAX_CLIENTS] = {};
    uint32_t stalledSince[MAX_CLIENTS] = {};
    char message[2560];

    // A slot holds a connection while it has a socket (connected() would
    // probe the socket on every call).
    bool used(int i) const { return clients[i].fd() >= 0; }
    void drop(int i)
    {
        clients[i].stop();
        clients[i] = WiFiClient();
    }
};
