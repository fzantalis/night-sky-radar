#include "NeoService.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>

#include "Net.h"
#include "TleFetcher.h"

namespace neoservice {

namespace {

// The cache holds the fetch timestamp on its own first line, followed by the
// raw response body.
//
// The timestamp lives in the file rather than in NVS on purpose. During M2 the
// TLE freshness stamp was kept in NVS while the data sat in LittleFS, and
// `pio run -t uploadfs` wipes one but not the other - the device then believed
// it had fresh data it no longer had, and went blind for a full refresh
// period. Keeping both halves in the same file makes that state unreachable.
constexpr const char* kCachePath = "/neo.json";
constexpr const char* kTempPath  = "/neo.json.tmp";

std::vector<neo::Approach> g_approaches;
int64_t g_fetchedAt = 0;      // 0 = nothing cached

// Guards against retrying a failing fetch on every single loop() iteration.
uint32_t g_lastAttemptMs = 0;
bool     g_everAttempted = false;
constexpr uint32_t RETRY_INTERVAL_MS = 60UL * 1000UL;

bool writeCache(int64_t fetchedAt, const String& body) {
    // Temp-and-rename, so a reset midway through a write cannot leave a
    // half-written file that would parse as garbage on the next boot.
    File f = LittleFS.open(kTempPath, "w");
    if (!f) {
        Serial.println("[neo] cache open failed");
        return false;
    }
    f.printf("%lld\n", static_cast<long long>(fetchedAt));
    const size_t written = f.print(body);
    f.close();

    if (written != body.length()) {
        Serial.printf("[neo] short write (%u of %u), keeping old cache\n",
                      static_cast<unsigned>(written),
                      static_cast<unsigned>(body.length()));
        LittleFS.remove(kTempPath);
        return false;
    }

    LittleFS.remove(kCachePath);
    if (!LittleFS.rename(kTempPath, kCachePath)) {
        Serial.println("[neo] cache rename failed");
        LittleFS.remove(kTempPath);
        return false;
    }
    return true;
}

// Fetches, parses, and only then commits. A body that does not parse as a CAD
// response never reaches the cache or the in-memory list.
bool fetchAndCommit(int64_t nowUnix) {
    String url = "https://ssd-api.jpl.nasa.gov/cad.api?dist-max=";
    url += String(static_cast<int>(NEO_RIM_LD));
    url += "LD&date-min=now&date-max=%2B";
    url += String(NEO_WINDOW_DAYS);
    url += "&sort=date&fullname=true";

    String body;
    bool got = false;

    // Scoped so TLS releases its ~40-50 KB before anything else allocates -
    // same discipline as the CelesTrak fetch.
    {
        WiFiClientSecure client;
        client.setCACert(tlefetcher::trustedRootsPem());
        client.setTimeout(15000);

        HTTPClient http;
        http.setConnectTimeout(15000);
        http.setTimeout(15000);
        http.setUserAgent("esp32-sky-radar/0.1");

        if (!http.begin(client, url)) {
            Serial.println("[neo] http begin failed");
            return false;
        }

        const int code = http.GET();
        if (code == HTTP_CODE_OK) {
            body = http.getString();
            got  = body.length() > 0;
        } else {
            // Unlike CelesTrak, this API reports its own errors with a real
            // status code and a {"code","message"} body, so a non-200 is
            // genuinely conclusive here.
            Serial.printf("[neo] GET failed, code %d\n", code);
        }
        http.end();
    }

    if (!got) return false;

    std::vector<neo::Approach> parsed;
    if (!neo::parseCad(body.c_str(), parsed, MAX_NEOS)) {
        Serial.printf("[neo] response did not parse as CAD (%u bytes), keeping cache\n",
                      static_cast<unsigned>(body.length()));
        return false;
    }

    // An authentic response listing nothing is a real answer, not a failure -
    // there are genuinely quiet stretches with no approach inside 10 lunar
    // distances. Committing it is correct; rejecting it would pin the display
    // to a stale list forever.
    if (!writeCache(nowUnix, body)) return false;

    g_approaches = parsed;
    g_fetchedAt  = nowUnix;
    Serial.printf("[neo] fetched %u bytes, %u approaches within %.0f LD over %d days\n",
                  static_cast<unsigned>(body.length()),
                  static_cast<unsigned>(parsed.size()),
                  NEO_RIM_LD, NEO_WINDOW_DAYS);
    return true;
}

void loadCache() {
    File f = LittleFS.open(kCachePath, "r");
    if (!f) {
        Serial.println("[neo] no cached close-approach data");
        return;
    }

    const String stamp = f.readStringUntil('\n');
    String body;
    body.reserve(f.size());
    while (f.available()) body += static_cast<char>(f.read());
    f.close();

    const int64_t at = static_cast<int64_t>(strtoll(stamp.c_str(), nullptr, 10));
    if (at <= 0) {
        Serial.println("[neo] cache has no usable timestamp, ignoring");
        return;
    }

    std::vector<neo::Approach> parsed;
    if (!neo::parseCad(body.c_str(), parsed, MAX_NEOS)) {
        Serial.println("[neo] cached body did not parse, ignoring");
        return;
    }

    g_approaches = parsed;
    g_fetchedAt  = at;
    Serial.printf("[neo] loaded %u cached approaches\n",
                  static_cast<unsigned>(parsed.size()));
}

bool refreshDue(int64_t nowUnix) {
    if (g_fetchedAt <= 0) return true;
    const double age = ageHours(nowUnix);
    if (age < 0.0) return true;     // clock moved backwards; treat as stale
    return age >= NEO_REFRESH_HOURS;
}

}  // namespace

void begin() {
    loadCache();
}

double ageHours(int64_t nowUnix) {
    if (g_fetchedAt <= 0) return -1.0;
    return static_cast<double>(nowUnix - g_fetchedAt) / 3600.0;
}

const std::vector<neo::Approach>& approaches() { return g_approaches; }

void loop(int64_t nowUnix) {
    // The project's standing rule: never act on a clock that has never been
    // set. `date-min=now` is resolved by JPL, but the cache timestamp and
    // every countdown derived from it are ours, and stamping them from a 1970
    // clock is what made M2's pass predictions rebase by 56 years.
    if (!net::timeValid() || !net::wifiUp()) return;
    if (!refreshDue(nowUnix)) return;

    const uint32_t now = millis();
    if (g_everAttempted && (now - g_lastAttemptMs) < RETRY_INTERVAL_MS) return;
    g_lastAttemptMs = now;
    g_everAttempted = true;

    fetchAndCommit(nowUnix);
}

}  // namespace neoservice
