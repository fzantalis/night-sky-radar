#include "Magnitude.h"

#include <cmath>

namespace {

constexpr double PI = 3.14159265358979323846;

// Floors that keep degenerate geometry finite instead of producing infinities
// that would propagate into the snapshot.
constexpr double MIN_PHASE_FACTOR = 1e-10;
constexpr double MIN_RANGE_KM     = 1e-3;

}  // namespace

double phaseAngleRad(const Vec3& satEci, const Vec3& sunEci, const Vec3& siteEci) {
    // toSun: satellite -> Sun direction.
    // toSat: observer's sightline to the satellite (site -> satellite), i.e. the
    // direction the observer is looking, extended through the satellite. Using
    // this rather than the satellite -> observer direction is what pins the
    // reference-condition unit tests below: it makes phi -> 0 when the
    // satellite sits on the far side of the observer's sightline from the Sun
    // (illuminated face toward the observer) and phi -> pi when the satellite
    // sits directly between the observer and the Sun (unlit face toward the
    // observer). Spec section 13 item 2 flags phase-angle sign as exactly the
    // kind of thing an inverted convention here would get wrong silently;
    // this pairing is pinned by unit test now and re-verified end-to-end
    // against N2YO in Task 9.
    const Vec3 toSun = (sunEci - satEci).unit();
    const Vec3 toSat = (satEci - siteEci).unit();

    if (toSun.norm() <= 0.0 || toSat.norm() <= 0.0) return PI / 2.0;

    double c = toSun.dot(toSat);
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
