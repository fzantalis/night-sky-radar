#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Hard cap from spec section 7.3. The core ranks and truncates; renderers never
// decide what matters.
constexpr int MAX_BLIPS = 12;

enum class ScopeStatus {
    Ok,
    NoTime,       // NTP has never succeeded; do not plot anything
    NoLocation,   // observer position unset
    Offline       // running from cached elements
};

struct Blip {
    int         id           = 0;
    std::string name;
    double      r            = 0.0;   // 0 = centre, 1 = rim - projection happens here, in the core
    double      theta        = 0.0;   // degrees, sky frame, 0 = true north
    double      elevationDeg = 0.0;   // degrees above horizon; r is derived from this via skyRadius()
    double      magnitude    = 99.0;
    bool        visible      = false; // reserved for M2; always false at M1
    std::string reason;    // visibility verdict reason, for debugging
    std::vector<std::pair<double, double>> trail;  // (r, theta) history

    // M3: "sat" for everything tracked before this milestone; "train" for a
    // member of a freshly-launched Starlink batch (see TrainFilter.h). The
    // renderer uses this to draw trains as a group rather than as unrelated
    // dots, instead of guessing from the name string.
    std::string kind         = "sat";

    // COSPAR launch designator pieces, straight from Tle::launchYear/
    // launchNumber - populated for every blip, not just trains, since the
    // core already has them. This is what lets a renderer group train
    // members under their shared launch rather than listing every one as an
    // unrelated row.
    int         launchYear   = 0;
    int         launchNumber = 0;
};

// One elevation ring the chrome draws around the dial. `r` is the projected
// radius (via skyRadius()) for `elevationDeg` - renderers plot it, they never
// compute it. `kind` distinguishes the visibility-floor ring and the horizon
// from the plain elevation grid so a renderer can keep them visually distinct
// without knowing which elevation angle means what.
struct Ring {
    double      elevationDeg = 0.0;
    double      r            = 0.0;
    std::string kind;   // "grid" | "floor" | "horizon"
};

// The rings ScopeService::build() (and any freshly-constructed Snapshot, e.g.
// in tests) draws by default: elevation 60/30 grid, the elevation-10
// visibility floor, and the elevation-0 horizon.
std::vector<Ring> defaultSkyRings();

// A predicted upcoming visible pass, as read from passtask::upcoming(). This
// is display data, already rebased onto the snapshot's own `t` - renderers
// never recompute a countdown, they just format one.
struct Event {
    std::string name;
    int64_t     startsIn = 0;     // seconds from the snapshot's t
    double      maxEl    = 0.0;
    bool        visible  = false;
};

struct Snapshot {
    int64_t     t            = 0;
    ScopeStatus status       = ScopeStatus::NoTime;
    double      tleAgeHours  = -1.0;   // negative means unknown
    std::vector<Blip> blips;
    std::vector<Ring> rings  = defaultSkyRings();
    double      sunAltDeg    = 0.0;
    std::vector<Event> events;
};

// Sorts visible objects first, then by ascending r (higher in the sky first),
// and truncates to MAX_BLIPS.
void rankAndCap(Snapshot& s);

std::string toJson(const Snapshot& s);
