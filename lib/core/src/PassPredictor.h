#pragma once

#include <cstdint>
#include <vector>

#include "Observer.h"
#include "Propagator.h"

struct Pass {
    int64_t riseUnix = 0;
    int64_t maxUnix  = 0;
    int64_t setUnix  = 0;
    double  maxElDeg = 0.0;
    bool    visible  = false;   // was the object naked-eye visible at maximum
};

// Called periodically during the search so a long prediction can yield to the
// scheduler. May be nullptr in native tests.
using YieldFn = void (*)();

// Cheap geometric rejection: can an orbit of this inclination and mean motion
// ever put the satellite above MIN_ELEVATION_DEG for an observer at this
// latitude? Discards candidates before any propagation happens.
bool couldEverRise(double inclinationDeg, double meanMotionRevPerDay, double obsLatDeg);

// Coarse 30-second stepping to bracket horizon crossings, then bisection to
// refine rise and set to about a second. Visibility is evaluated at maximum
// elevation, which is when a pass is at its brightest.
//
// Takes `prop` by non-const reference: Propagator::positionAt() is
// deliberately non-const (see Propagator.h), so a const reference here would
// not compile. A `Propagator&&` overload is provided below for the common
// case of a temporary constructed for a single prediction call.
std::vector<Pass> predictPasses(Propagator& prop,
                                const Observer& obs,
                                double stdMag,
                                int64_t fromUnix,
                                int horizonHours,
                                YieldFn onYield);

inline std::vector<Pass> predictPasses(Propagator&& prop,
                                       const Observer& obs,
                                       double stdMag,
                                       int64_t fromUnix,
                                       int horizonHours,
                                       YieldFn onYield) {
    return predictPasses(prop, obs, stdMag, fromUnix, horizonHours, onYield);
}
