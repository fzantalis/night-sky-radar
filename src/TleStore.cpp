#include "TleStore.h"

#include <LittleFS.h>
#include <Preferences.h>

namespace {
constexpr const char* TLE_PATH = "/tle_stations.txt";
Preferences store;
}  // namespace

namespace tlestore {

void begin() {
    store.begin("tlestore", false);
}

bool save(const String& raw) {
    File f = LittleFS.open(TLE_PATH, "w");
    if (!f) {
        Serial.println("[tle] cache open for write failed");
        return false;
    }
    const size_t written = f.print(raw);
    f.close();

    if (written != raw.length()) {
        Serial.println("[tle] cache short write");
        return false;
    }
    return true;
}

String load() {
    File f = LittleFS.open(TLE_PATH, "r");
    if (!f) return String();
    String raw = f.readString();
    f.close();
    return raw;
}

void markFetched(int64_t nowUnix) {
    store.putLong64("last", nowUnix);
}

int64_t lastFetchUnix() {
    return store.getLong64("last", 0);
}

double ageHours(int64_t nowUnix) {
    const int64_t last = lastFetchUnix();
    if (last <= 0) return -1.0;
    return static_cast<double>(nowUnix - last) / 3600.0;
}

}  // namespace tlestore
