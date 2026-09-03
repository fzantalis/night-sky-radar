#include "HttpApi.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>

namespace {

WebServer server(80);
httpapi::SnapshotProvider snapshotProvider = nullptr;

void handleScope() {
    if (snapshotProvider == nullptr) {
        server.send(503, "application/json", "{\"status\":\"no_provider\"}");
        return;
    }
    const Snapshot s = snapshotProvider();
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", toJson(s).c_str());
}

bool serveStatic(const String& path, const char* mime) {
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    server.streamFile(f, mime);
    f.close();
    return true;
}

}  // namespace

namespace httpapi {

void begin(SnapshotProvider provider) {
    snapshotProvider = provider;

    if (!LittleFS.begin(false)) {
        Serial.println("[http] LittleFS mount failed - did you run 'pio run -t uploadfs'?");
    }

    server.on("/",          []() { if (!serveStatic("/index.html", "text/html"))       server.send(404, "text/plain", "no index.html"); });
    server.on("/index.html",[]() { if (!serveStatic("/index.html", "text/html"))       server.send(404, "text/plain", "not found"); });
    server.on("/radar.css", []() { if (!serveStatic("/radar.css",  "text/css"))        server.send(404, "text/plain", "not found"); });
    server.on("/radar.js",  []() { if (!serveStatic("/radar.js",   "application/javascript")) server.send(404, "text/plain", "not found"); });
    server.on("/api/scope", handleScope);

    server.onNotFound([]() { server.send(404, "text/plain", "not found"); });

    server.begin();
    Serial.println("[http] server started on port 80");
}

void loop() {
    server.handleClient();
}

}  // namespace httpapi
