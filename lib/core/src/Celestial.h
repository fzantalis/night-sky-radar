#pragma once

#include <cstdint>

#include "Observer.h"
#include "Topocentric.h"

// Converts a fixed equatorial position (right ascension, declination, both
// degrees, J2000) to the observer's horizontal frame at a given instant.
// Radiants are fixed stars for this purpose - no propagation involved, unlike
// look() in Topocentric.h which handles a moving satellite at finite range.
//
// A meteor radiant sits effectively at infinite distance, so only its
// direction matters - `LookAngles::rangeKm` is meaningless here and left at
// whatever the formula happens to produce; callers must not read it.
LookAngles radiantLookAngles(double raDeg, double decDeg,
                              const Observer& obs, int64_t unixSeconds);
