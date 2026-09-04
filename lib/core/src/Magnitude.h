#pragma once
#include "Vec3.h"

// Sun-satellite-observer phase angle in radians, in [0, pi]: the angle at the
// satellite between the direction to the Sun and the direction to the
// observer. 0 when the satellite is fully lit as seen by the observer (Sun
// behind the observer), pi when the satellite is backlit (satellite between
// observer and Sun). Pinned by the matched pair of unit tests in
// test/test_magnitude/test_magnitude.cpp.
double phaseAngleRad(const Vec3& satEci, const Vec3& sunEci, const Vec3& siteEci);

// Apparent visual magnitude.
//
//   mag = stdMag + 5*log10(range_km / 1000) - 2.5*log10(phaseFactor)
//   phaseFactor = (pi - phi)*cos(phi) + sin(phi)
//
// A standard magnitude is defined as the object's brightness at 1000 km range
// and 90 degrees phase, and this normalisation reproduces exactly that: at
// phi = pi/2 the phase factor is 1 and the range term is 0, so the result is
// stdMag. That identity is asserted by test and must not be broken.
double apparentMagnitude(double stdMag, double rangeKm, double phaseRad);
