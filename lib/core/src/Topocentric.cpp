#include "Topocentric.h"

#include <cmath>

namespace {

constexpr double PI      = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;

// WGS84 ellipsoid.
constexpr double WGS84_A  = 6378.137;              // equatorial radius, km
constexpr double WGS84_F  = 1.0 / 298.257223563;   // flattening
const     double WGS84_E2 = 2.0 * WGS84_F - WGS84_F * WGS84_F;

}  // namespace

Vec3 siteEci(const Observer& obs, double gmstDeg) {
    const double lat = obs.latDeg * DEG2RAD;
    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);

    // Radius of curvature in the prime vertical.
    const double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);

    // Local sidereal time: Greenwich sidereal time plus east longitude.
    const double lst = (gmstDeg + obs.lonDeg) * DEG2RAD;

    const double rxy = (N + obs.altKm) * cosLat;
    const double z   = (N * (1.0 - WGS84_E2) + obs.altKm) * sinLat;

    return Vec3{rxy * std::cos(lst), rxy * std::sin(lst), z};
}

LookAngles look(const Vec3& satTemeKm, const Observer& obs, double gmstDeg) {
    const Vec3 site = siteEci(obs, gmstDeg);
    const Vec3 d    = satTemeKm - site;   // range vector, inertial frame

    const double lat = obs.latDeg * DEG2RAD;
    const double lst = (gmstDeg + obs.lonDeg) * DEG2RAD;

    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinLst = std::sin(lst);
    const double cosLst = std::cos(lst);

    // Rotate the range vector into the topocentric SEZ frame:
    // S points south, E points east, Z points up along the local vertical.
    const double rS =  sinLat * cosLst * d.x + sinLat * sinLst * d.y - cosLat * d.z;
    const double rE = -sinLst * d.x + cosLst * d.y;
    const double rZ =  cosLat * cosLst * d.x + cosLat * sinLst * d.y + sinLat * d.z;

    LookAngles la;
    la.rangeKm = d.norm();

    if (la.rangeKm <= 0.0) return la;

    // rZ (a three-term SEZ sum) and rangeKm (a separate sqrt of a dot
    // product) are computed by independent floating-point paths, so they are
    // only *mathematically* guaranteed to satisfy |rZ| <= rangeKm. Near
    // zenith, rounding can push the ratio an ulp past 1.0, and asin() of
    // anything outside [-1, 1] is NaN. Clamp before asin, matching the
    // acos clamp in Magnitude.cpp for the same reason.
    double sinEl = rZ / la.rangeKm;
    if (sinEl > 1.0)  sinEl = 1.0;
    if (sinEl < -1.0) sinEl = -1.0;
    la.elDeg = std::asin(sinEl) * RAD2DEG;

    // Azimuth measured from north, increasing toward east. Negating the south
    // component turns SEZ into a north-referenced frame.
    double az = std::atan2(rE, -rS) * RAD2DEG;
    if (az < 0.0) az += 360.0;
    if (az >= 360.0) az -= 360.0;
    la.azDeg = az;

    return la;
}
