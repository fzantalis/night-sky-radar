#include "TleStore.h"

#include <LittleFS.h>
#include <Preferences.h>

namespace {

Preferences store;

String pathFor(const char* group) {
    String p = "/tle_";
    p += group;
    p += ".txt";
    return p;
}

String tmpPathFor(const char* group) {
    String p = "/tle_";
    p += group;
    p += ".tmp";
    return p;
}

}  // namespace

namespace tlestore {

void begin() {
    store.begin("tlestore", false);
}

// Writes to a temp path and renames over the real cache only once the write
// has fully succeeded. Opening TLE_PATH directly in "w" mode would truncate
// the existing good cache the instant this is called, so a short write or a
// write of bad content (see ScopeService::attemptRefresh, which validates the
// parsed content before ever calling this) could otherwise leave the device
// with no usable cache at all. The rename is atomic on LittleFS.
bool save(const char* group, const String& raw) {
    const String tmpPath = tmpPathFor(group);
    const String path    = pathFor(group);

    if (LittleFS.exists(tmpPath)) {
        LittleFS.remove(tmpPath);   // stale leftover from a prior failed write
    }

    File f = LittleFS.open(tmpPath, "w");
    if (!f) {
        Serial.println("[tle] cache open for write failed");
        return false;
    }
    const size_t written = f.print(raw);
    f.close();

    if (written != raw.length()) {
        Serial.println("[tle] cache short write");
        LittleFS.remove(tmpPath);
        return false;
    }

    if (!LittleFS.rename(tmpPath, path)) {
        Serial.println("[tle] cache rename failed");
        LittleFS.remove(tmpPath);
        return false;
    }
    return true;
}

String load(const char* group) {
    File f = LittleFS.open(pathFor(group), "r");
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
