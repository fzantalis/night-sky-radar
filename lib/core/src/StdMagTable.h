#pragma once

// Standard magnitude: apparent visual magnitude at 1000 km range and 90 degrees
// phase angle. See docs/third-party/STDMAG-PROVENANCE.md for sourcing and for
// the reasoning behind the default.
constexpr double DEFAULT_STD_MAG = 2.5;

double stdMagFor(int noradId);
