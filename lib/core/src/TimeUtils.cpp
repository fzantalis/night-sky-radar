#include "TimeUtils.h"
#include <cmath>

namespace timeutils {

double julianDate(int64_t unixSeconds) {
    return JD_UNIX_EPOCH + static_cast<double>(unixSeconds) / 86400.0;
}

double gmstDegrees(double jd) {
    // Vallado, "Fundamentals of Astrodynamics and Applications", GMST in
    // seconds of a sidereal day, then converted to degrees (86400 s = 360 deg,
    // so 240 seconds per degree).
    const double T = (jd - JD_J2000) / 36525.0;

    double sec = 67310.54841
               + (876600.0 * 3600.0 + 8640184.812866) * T
               + 0.093104 * T * T
               - 6.2e-6 * T * T * T;

    sec = std::fmod(sec, 86400.0);
    if (sec < 0.0) sec += 86400.0;

    return sec / 240.0;
}

}  // namespace timeutils
