#include "Celestial.h"

#include <cmath>

#include "TimeUtils.h"

namespace {

constexpr double PI      = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;

}  // namespace

// Standard equatorial-to-horizontal transform (e.g. Vallado, "Fundamentals of
// Astrodynamics and Applications"):
//
//   LST = GMST + observer longitude        (east longitude positive, matching
//                                            siteEci()'s convention in
//                                            Topocentric.cpp)
//   H   = LST - RA                          (hour angle)
//   sin(el) = sin(dec)sin(lat) + cos(dec)cos(lat)cos(H)
//   az = atan2(-sin(H)cos(dec), cos(lat)sin(dec) - sin(lat)cos(dec)cos(H))
//        normalised to [0, 360), measured from north increasing eastward
//
// This is algebraically the same rotation Topocentric.cpp's look() applies to
// a satellite's range vector, specialised to a direction vector at infinite
// range: substituting d = (cos(dec)cos(RA), cos(dec)sin(RA), sin(dec)) - the
// radiant's unit direction in the equatorial frame - into look()'s rS/rE/rZ
// and simplifying with the angle-sum identities reduces exactly to the
// formulas above. That gives both formulas the same tested sign convention
// rather than a second, independently-derived one.
LookAngles radiantLookAngles(double raDeg, double decDeg,
                              const Observer& obs, int64_t unixSeconds) {
    const double gmst = timeutils::gmstDegrees(timeutils::julianDate(unixSeconds));

    const double lat = obs.latDeg * DEG2RAD;
    const double dec = decDeg * DEG2RAD;

    double H = gmst + obs.lonDeg - raDeg;
    H = std::fmod(H, 360.0);
    if (H < 0.0) H += 360.0;
    const double hRad = H * DEG2RAD;

    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinDec = std::sin(dec);
    const double cosDec = std::cos(dec);
    const double sinH   = std::sin(hRad);
    const double cosH   = std::cos(hRad);

    // Clamp before asin: see the identical guard (and its rationale) in
    // Topocentric.cpp's look() and Magnitude.cpp's phaseAngleRad(). The
    // algebraic identity with look() above means this clamp closes off the
    // same rounding path, at the celestial pole and the zenith respectively.
    double sinEl = sinDec * sinLat + cosDec * cosLat * cosH;
    if (sinEl > 1.0)  sinEl = 1.0;
    if (sinEl < -1.0) sinEl = -1.0;

    LookAngles la;
    la.elDeg = std::asin(sinEl) * RAD2DEG;

    double az = std::atan2(-sinH * cosDec, cosLat * sinDec - sinLat * cosDec * cosH) * RAD2DEG;
    if (az < 0.0)    az += 360.0;
    if (az >= 360.0) az -= 360.0;
    la.azDeg = az;

    return la;
}
