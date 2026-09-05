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

// M5. Design spec section 3.1: "SKY mode and NEO mode are the same renderer fed
// different projections, rather than two separate screens." The mode travels in
// the snapshot so a renderer knows which axes it is drawing, and nothing else.
enum class ScopeMode {
    Sky,   // radius = elevation, angle = azimuth. Where to look, right now.
    Neo    // radius = miss distance, angle = when. How close, and when.
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
    std::string kind;   // "grid" | "floor" | "horizon" | "moon"

    // What to print against the ring. Empty means "derive it from
    // elevationDeg", which is what every SKY-mode ring does. NEO mode sets it
    // explicitly ("1 LD", "10 LD"), because there the radius is a distance and
    // elevationDeg is meaningless.
    std::string label;
};

// The rings ScopeService::build() (and any freshly-constructed Snapshot, e.g.
// in tests) draws by default: elevation 60/30 grid, the elevation-10
// visibility floor, and the elevation-0 horizon.
std::vector<Ring> defaultSkyRings();

// NEO-mode rings, scaled so `rimLd` lunar distances lands on the rim. The
// 1 LD ring is marked "moon" rather than "grid": it is the single reference
// everyone already has an intuition for, and an approach inside it is the
// thing actually worth noticing.
std::vector<Ring> defaultNeoRings(double rimLd);

// A predicted upcoming visible pass, as read from passtask::upcoming(). This
// is display data, already rebased onto the snapshot's own `t` - renderers
// never recompute a countdown, they just format one.
struct Event {
    std::string name;
    int64_t     startsIn = 0;     // seconds from the snapshot's t
    double      maxEl    = 0.0;
    bool        visible  = false;
};

// A meteor shower radiant currently worth showing on the dial. Unlike a Blip,
// this is not an object to point at - it is a region of sky to watch, drawn
// as a soft glow rather than a hard dot (see M4 plan Task 3). Only included
// when the shower is active, the radiant is above the horizon, and the sun is
// below MAX_SUN_ALT_DEG - ScopeService::build() enforces all three.
struct Radiant {
    std::string name;
    double      r        = 0.0;   // same projection convention as Blip::r
    double      theta    = 0.0;   // degrees, sky frame, 0 = true north
    double      elevationDeg = 0.0;
    int         zhr      = 0;
    bool        atPeak   = false;
};

// M5 - one asteroid close approach, already projected onto the dial by
// neo::project(). Kept separate from Blip rather than overloading it: a Blip
// carries an elevation, a visibility verdict and a ground track, none of which
// mean anything for an event that has not happened yet. The renderer stays
// shared because the projection is shared, not because the struct is.
struct NeoApproachBlip {
    std::string name;        // designation, e.g. "2026 RG"
    std::string fullname;
    double  r          = 0.0;   // same normalised convention as Blip::r
    double  theta      = 0.0;   // degrees; 0 = now, increasing with time
    double  distLd     = 0.0;   // miss distance in lunar distances
    double  vRelKmS    = 0.0;
    double  hMag       = 99.0;  // absolute magnitude
    bool    hKnown     = false;
    int64_t approachIn = 0;     // seconds from the snapshot's t; may be negative

    // Rough size from absolute magnitude, assuming a 0.14 geometric albedo:
    //   D(km) = 1329 / sqrt(albedo) * 10^(-H/5)
    // This is the standard relation (Bowell et al., "Asteroids II", 1989) and
    // 0.14 is the conventional default for an unmeasured NEO. It is an
    // order-of-magnitude figure, not a measurement - a dark object can be
    // twice this and a bright one half - but "about 11 m" communicates far
    // more than "H = 27.56". Zero when H is unknown.
    double  estimatedDiameterM = 0.0;
};

// Diameter estimate used by NeoApproachBlip::estimatedDiameterM, exposed so it
// can be tested directly. Returns 0 for a non-finite or absent magnitude.
double estimatedDiameterMetres(double hMag, double albedo = 0.14);

struct Snapshot {
    int64_t     t            = 0;
    ScopeMode   mode         = ScopeMode::Sky;
    ScopeStatus status       = ScopeStatus::NoTime;
    double      tleAgeHours  = -1.0;   // negative means unknown
    std::vector<Blip> blips;
    std::vector<Ring> rings  = defaultSkyRings();
    double      sunAltDeg    = 0.0;
    std::vector<Event> events;
    std::vector<Radiant> radiants;

    // Populated only in NEO mode; `blips` is empty there and vice versa.
    std::vector<NeoApproachBlip> neos;

    // How many lunar distances the rim represents in NEO mode. Travels with
    // the snapshot so the renderer can label the dial without knowing the
    // fetch parameters.
    double      neoRimLd     = 10.0;

    // Age of the cached close-approach data, hours. Negative means unknown.
    double      neoAgeHours  = -1.0;
};

// Sorts visible objects first, then by ascending r (higher in the sky first),
// and truncates to MAX_BLIPS.
void rankAndCap(Snapshot& s);

std::string toJson(const Snapshot& s);
