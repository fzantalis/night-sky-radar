#include "Solar.h"

#include <cmath>

#include "TimeUtils.h"
#include "Topocentric.h"

namespace {

constexpr double PI      = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;
constexpr double AU_KM   = 149597870.7;

double norm360(double d) {
    d = std::fmod(d, 360.0);
    if (d < 0.0) d += 360.0;
    return d;
}

}  // namespace

Vec3 sunEci(double jd) {
    const double n = jd - timeutils::JD_J2000;

    const double L = norm360(280.460 + 0.9856474 * n);   // mean longitude
    const double g = norm360(357.528 + 0.9856003 * n);   // mean anomaly
    const double gRad = g * DEG2RAD;

    // Apparent ecliptic longitude: mean longitude plus the equation of centre.
    const double lambda =
        (L + 1.915 * std::sin(gRad) + 0.020 * std::sin(2.0 * gRad)) * DEG2RAD;

    const double eps = (23.439 - 0.0000004 * n) * DEG2RAD;   // obliquity

    // Distance in astronomical units, then kilometres.
    const double rAu = 1.00014
                     - 0.01671 * std::cos(gRad)
                     - 0.00014 * std::cos(2.0 * gRad);
    const double r = rAu * AU_KM;

    // Ecliptic to equatorial.
    return Vec3{ r * std::cos(lambda),
                 r * std::cos(eps) * std::sin(lambda),
                 r * std::sin(eps) * std::sin(lambda) };
}

double sunDeclinationDeg(double jd) {
    const Vec3 s = sunEci(jd);
    const double r = s.norm();
    if (r <= 0.0) return 0.0;
    return std::asin(s.z / r) * RAD2DEG;
}

double sunAltitudeDeg(const Observer& obs, int64_t unixSeconds) {
    const double jd   = timeutils::julianDate(unixSeconds);
    const double gmst = timeutils::gmstDegrees(jd);
    // Reuse the already-tested topocentric transform rather than repeating it.
    return look(sunEci(jd), obs, gmst).elDeg;
}
