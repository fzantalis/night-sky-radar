#pragma once

#include <cstdint>
#include <string>
#include <vector>

// M5 - near-Earth object close approaches, from JPL's SBDB CAD API.
//
// This is a different kind of object from everything else on the dial. A
// satellite is somewhere in the sky *right now* and the dial answers "where do
// I look". An asteroid close approach is an event at a *time*, and the
// interesting question is "how close, and when". So NEO mode reuses the polar
// dial with a different meaning for both axes:
//
//   radius = miss distance, centre = Earth, rim = `rimLd` lunar distances
//   angle  = when it happens across the window, 0 = now, clockwise
//
// Projection still happens here, in the core, exactly as it does for
// satellites (design spec section 3.1) - renderers stay dumb polar plotters
// and gain no new geometry.
namespace neo {

// Mean lunar distance expressed in astronomical units: 384,400 km / AU, with
// the AU at its IAU 2012 definition of 149,597,870.7 km. The CAD API reports
// distances in au, and the dial is scaled in lunar distances, so every
// conversion in this file goes through this one constant.
constexpr double AU_PER_LD = 384400.0 / 149597870.7;

struct Approach {
    std::string des;       // designation, e.g. "2026 RG"
    std::string fullname;  // formatted name when the request asked for it
    double  jd           = 0.0;   // close-approach time, Julian date (TDB)
    int64_t approachUnix = 0;     // same instant, seconds since the Unix epoch
    double  distAu       = 0.0;   // nominal miss distance
    double  distLd       = 0.0;   // the same, in lunar distances
    double  vRelKmS      = 0.0;   // relative velocity at approach
    double  hMag         = 99.0;  // absolute magnitude; smaller is bigger
    bool    hKnown       = false; // the API returns null for some objects
};

// Converts a Julian date to Unix seconds. The CAD API's `jd` is TDB, which
// runs a bit over a minute ahead of UTC; that is deliberately ignored, because
// the dial's angular resolution over a 30-day window is about 2 hours per
// degree and the offset is far below one pixel.
int64_t julianDateToUnix(double jd);

// Parses a JPL SBDB CAD API response.
//
// Returns false when the payload is not a CAD response at all - no `fields`
// array, no `data` array, or a `fields` array without the columns we need.
// That is the parse-before-commit discipline this project already applies to
// CelesTrak: a captive portal or an error page served as HTTP 200 must be
// rejected *before* it can replace good data. An authentic response carrying
// zero approaches is a success returning an empty vector, not a failure.
bool parseCad(const char* json, std::vector<Approach>& out, int maxOut);

// Maps one approach onto the dial. `r` is clamped to [0,1]; an approach
// outside the window is still projected, so a caller can decide whether to
// drop it rather than having that decision hidden here.
void project(const Approach& a, int64_t nowUnix, double windowDays,
             double rimLd, double& r, double& theta);

}  // namespace neo
