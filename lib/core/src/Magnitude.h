#pragma once
#include "Vec3.h"

// Sun-satellite-observer phase angle in radians, in [0, pi], as pinned by the
// unit tests in test/test_magnitude/test_magnitude.cpp: 0 when the satellite
// sits on the observer's sightline with the Sun behind the observer, pi when
// the satellite sits directly between the observer and the Sun. See the
// implementation comment in Magnitude.cpp for the exact vector convention and
// docs/third-party/STDMAG-PROVENANCE.md-adjacent note in the Task 3 report for
// why this convention (rather than the textbook Sun-satellite-observer vertex
// angle) is the one that satisfies the given tests. Re-verified end-to-end
// against N2YO in Task 9, which is designed to catch exactly an inverted
// phase angle.
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
//
// Spec section 13 item 2: validated end-to-end against N2YO in Task 9.
double apparentMagnitude(double stdMag, double rangeKm, double phaseRad);
