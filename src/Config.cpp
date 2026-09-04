#include "Config.h"

#include <Preferences.h>
#include <cmath>

namespace {
Preferences prefs;
constexpr const char* NS = "skyradar";
}  // namespace

namespace config {

void begin() {
    prefs.begin(NS, false);
}

bool hasLocation() {
    if (!prefs.isKey("lat") || !prefs.isKey("lon")) return false;

    // A stored value can only get here as NaN/inf if it was written before
    // HttpApi.cpp's input validation existed, or by something writing NVS
    // directly. Either way, trusting it produces a confident wrong sky (or,
    // for NaN, blips that bypass the horizon filter and break /api/scope's
    // JSON). Refuse to call it a location unless it is finite and in range,
    // so a poisoned value degrades to NoLocation - recoverable through the
    // /config form - instead of a permanently broken device.
    const double lat = prefs.getDouble("lat", 0.0);
    const double lon = prefs.getDouble("lon", 0.0);
    const double alt = prefs.getDouble("alt", 0.0);

    if (!std::isfinite(lat) || lat < -90.0  || lat > 90.0)  return false;
    if (!std::isfinite(lon) || lon < -180.0 || lon > 180.0) return false;
    if (!std::isfinite(alt) || alt < -0.5   || alt > 9.0)   return false;

    return true;
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
