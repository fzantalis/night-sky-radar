#pragma once

#include "Vec3.h"
#include "Observer.h"

struct LookAngles {
    double azDeg    = 0.0;   // [0, 360), 0 = true north, increasing eastward
    double elDeg    = 0.0;   // [-90, 90]
    double rangeKm  = 0.0;
};

// Observer's position in the same inertial frame SGP4 outputs (TEME), obtained
// by rotating the Earth-fixed site vector by GMST.
Vec3 siteEci(const Observer& obs, double gmstDeg);

// Look angles from the observer to a satellite. `satTemeKm` must come from
// Propagator::positionAt and `gmstDeg` from timeutils::gmstDegrees for the same
// instant.
LookAngles look(const Vec3& satTemeKm, const Observer& obs, double gmstDeg);
