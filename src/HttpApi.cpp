#include "HttpApi.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Config.h"
#include "ScopeService.h"

namespace {

// String::toDouble() is a bare atof()/strtod(s, NULL): it has no way to
// reject NaN, +-inf, trailing garbage ("12abc") or a fully non-numeric value
// ("abc", ""), all of which strtod happily turns into either a parsed
// "number" or a silent 0.0. Parse by hand instead so we can refuse anything
// that isn't a clean, finite number.
bool parseFinite(const String& s, double& out) {
    String t = s;
    t.trim();
    if (t.length() == 0) return false;

    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (end == t.c_str() || *end != '\0') return false;  // no parse, or trailing garbage
    if (!std::isfinite(v)) return false;                 // NaN / +inf / -inf

    out = v;
    return true;
}

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

void handleConfigGet() {
    const Observer o = config::observer();
    char buf[128];
    if (config::hasLocation()) {
        std::snprintf(buf, sizeof(buf),
                      "{\"lat\":%.6f,\"lon\":%.6f,\"alt\":%.4f}",
                      o.latDeg, o.lonDeg, o.altKm);
    } else {
        std::snprintf(buf, sizeof(buf), "{}");
    }
    server.send(200, "application/json", buf);
}

void handleConfigPost() {
    if (!server.hasArg("lat") || !server.hasArg("lon") || !server.hasArg("alt")) {
        server.send(400, "text/plain", "missing lat, lon or alt");
        return;
    }

    double lat, lon, alt;

    // Reject anything that doesn't parse as a clean finite number BEFORE the
    // range checks below - a NaN or inf comparison against a range is always
    // false, so the range checks alone cannot see (and would silently admit)
    // "nan", "inf", "-inf", or garbage like "abc"/"12abc"/"" that toDouble()
    // would otherwise turn into 0.0.
    if (!parseFinite(server.arg("lat"), lat)) { server.send(400, "text/plain", "lat is not a valid number"); return; }
    if (!parseFinite(server.arg("lon"), lon)) { server.send(400, "text/plain", "lon is not a valid number"); return; }
    if (!parseFinite(server.arg("alt"), alt)) { server.send(400, "text/plain", "alt is not a valid number"); return; }

    // Reject out-of-range values rather than storing a position that would
    // silently produce a wrong sky.
    if (lat < -90.0 || lat > 90.0)    { server.send(400, "text/plain", "lat out of range"); return; }
    if (lon < -180.0 || lon > 180.0)  { server.send(400, "text/plain", "lon out of range"); return; }
    if (alt < -0.5 || alt > 9.0)      { server.send(400, "text/plain", "alt out of range (km)"); return; }

    config::setObserver(Observer{lat, lon, alt});
    scope::clearTrails();
    Serial.printf("[cfg] observer set to %.4f, %.4f, %.3f km\n", lat, lon, alt);

    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "saved");
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
    server.on("/config",     []() { if (!serveStatic("/config.html", "text/html")) server.send(404, "text/plain", "not found"); });
    server.on("/api/config", HTTP_GET,  handleConfigGet);
    server.on("/api/config", HTTP_POST, handleConfigPost);

    server.onNotFound([]() { server.send(404, "text/plain", "not found"); });

    server.begin();
    Serial.println("[http] server started on port 80");
}

void loop() {
    server.handleClient();
}

}  // namespace httpapi
