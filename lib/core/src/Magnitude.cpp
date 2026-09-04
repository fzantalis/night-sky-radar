#include "Magnitude.h"

#include <cmath>

namespace {

constexpr double PI = 3.14159265358979323846;

// Floors that keep degenerate geometry finite instead of producing infinities
// that would propagate into the snapshot.
//
// MIN_PHASE_FACTOR: phaseFactor -> 0 as phaseRad -> pi (fully backlit), which
// would otherwise send -2.5*log10(phaseFactor) to +infinity. This floor is a
// numerical-safety bound, not tuned to any test assertion: it just needs to
// (a) stay well inside the domain where log10 is finite and (b) be small
// enough that a backlit object's magnitude lands far past any plausible
// visibility threshold, so it reads as "unambiguously too dim" rather than
// producing a borderline number.
constexpr double MIN_PHASE_FACTOR = 1e-10;
constexpr double MIN_RANGE_KM     = 1e-3;

}  // namespace

double phaseAngleRad(const Vec3& satEci, const Vec3& sunEci, const Vec3& siteEci) {
    // Phase angle is measured at the satellite, between the direction to the
    // Sun and the direction to the observer.
    const Vec3 toSun = (sunEci - satEci).unit();
    const Vec3 toObs = (siteEci - satEci).unit();

    if (toSun.norm() <= 0.0 || toObs.norm() <= 0.0) return PI / 2.0;

    double c = toSun.dot(toObs);
    if (c > 1.0) c = 1.0;
    if (c < -1.0) c = -1.0;

    return std::acos(c);
}

double apparentMagnitude(double stdMag, double rangeKm, double phaseRad) {
    if (phaseRad < 0.0) phaseRad = 0.0;
    if (phaseRad > PI)  phaseRad = PI;

    double phaseFactor = (PI - phaseRad) * std::cos(phaseRad) + std::sin(phaseRad);
    if (phaseFactor < MIN_PHASE_FACTOR) phaseFactor = MIN_PHASE_FACTOR;

    double range = rangeKm;
    if (range < MIN_RANGE_KM) range = MIN_RANGE_KM;

    return stdMag
         + 5.0 * std::log10(range / 1000.0)
         - 2.5 * std::log10(phaseFactor);
}
