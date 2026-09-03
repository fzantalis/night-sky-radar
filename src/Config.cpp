#include "Config.h"

#include <Preferences.h>

namespace {
Preferences prefs;
constexpr const char* NS = "skyradar";
}  // namespace

namespace config {

void begin() {
    prefs.begin(NS, false);
}

bool hasLocation() {
    return prefs.isKey("lat") && prefs.isKey("lon");
}

Observer observer() {
    Observer o;
    o.latDeg = prefs.getDouble("lat", 0.0);
    o.lonDeg = prefs.getDouble("lon", 0.0);
    o.altKm  = prefs.getDouble("alt", 0.0);
    return o;
}

void setObserver(const Observer& obs) {
    prefs.putDouble("lat", obs.latDeg);
    prefs.putDouble("lon", obs.lonDeg);
    prefs.putDouble("alt", obs.altKm);
}

}  // namespace config
