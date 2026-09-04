#pragma once

#include <cstdint>
#include "Vec3.h"
#include "Observer.h"

// Geocentric position of the Sun in kilometres, in the same equatorial frame
// the propagator outputs. Uses the low-precision Astronomical Almanac series,
// good to roughly 0.01 degrees - far better than this application needs.
Vec3 sunEci(double jd);

double sunDeclinationDeg(double jd);

// Altitude of the Sun above the observer's horizon, in degrees. Negative means
// below the horizon. Spec section 5 requires this to be below -6 for an object
// to count as visible.
double sunAltitudeDeg(const Observer& obs, int64_t unixSeconds);
