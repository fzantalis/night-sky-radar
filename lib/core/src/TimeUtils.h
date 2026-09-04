#pragma once
#include <cstdint>

namespace timeutils {

// Julian date of the unix epoch, 1970-01-01T00:00:00Z.
constexpr double JD_UNIX_EPOCH = 2440587.5;

// Julian date of J2000.0, 2000-01-01T12:00:00Z.
constexpr double JD_J2000 = 2451545.0;

double julianDate(int64_t unixSeconds);

// Greenwich Mean Sidereal Time in degrees, normalised to [0, 360).
double gmstDegrees(double jd);

}  // namespace timeutils
