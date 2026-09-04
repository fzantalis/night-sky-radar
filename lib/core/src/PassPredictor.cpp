#include "PassPredictor.h"

#include <cmath>

#include "Eclipse.h"
#include "Solar.h"
#include "TimeUtils.h"
#include "Topocentric.h"
#include "Visibility.h"

namespace {

constexpr double PI       = 3.14159265358979323846;
constexpr double DEG2RAD  = PI / 180.0;
constexpr double RAD2DEG  = 180.0 / PI;
constexpr double MU       = 398600.4418;   // km^3/s^2

constexpr int COARSE_STEP_SEC   = 30;
constexpr int BISECTION_ROUNDS  = 12;      // 30s / 2^12 is well under a second
constexpr int YIELD_EVERY_STEPS = 200;

// Elevation of the satellite above the observer's horizon at a given instant,
// or a large negative number if the propagator failed.
double elevationAt(Propagator& prop, const Observer& obs, int64_t t) {
    Vec3 pos;
    if (!prop.positionAt(t, pos)) return -999.0;
    const double gmst = timeutils::gmstDegrees(timeutils::julianDate(t));
    return look(pos, obs, gmst).elDeg;
}

// Finds the instant between `lo` and `hi` where elevation crosses the floor.
// Requires the two ends to straddle the crossing.
int64_t refineCrossing(Propagator& prop, const Observer& obs,
                       int64_t lo, int64_t hi) {
    for (int i = 0; i < BISECTION_ROUNDS && hi - lo > 1; ++i) {
        const int64_t mid = lo + (hi - lo) / 2;
        if (elevationAt(prop, obs, mid) < MIN_ELEVATION_DEG) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return hi;
}

}  // namespace

bool couldEverRise(double inclinationDeg, double meanMotionRevPerDay, double obsLatDeg) {
    if (meanMotionRevPerDay <= 0.0) return false;

    // Semi-major axis from mean motion, then altitude above a spherical Earth.
    const double nRadPerSec = meanMotionRevPerDay * 2.0 * PI / 86400.0;
    const double a = std::cbrt(MU / (nRadPerSec * nRadPerSec));
    const double h = a - EARTH_RADIUS_KM;
    if (h <= 0.0) return false;

    // Maximum geocentric angle from the sub-satellite point at which the object
    // still sits above the elevation floor.
    const double elRad = MIN_ELEVATION_DEG * DEG2RAD;
    double c = (EARTH_RADIUS_KM / (EARTH_RADIUS_KM + h)) * std::cos(elRad);
    if (c > 1.0) c = 1.0;
    if (c < -1.0) c = -1.0;
    const double lambdaMaxDeg = (std::acos(c) - elRad) * RAD2DEG;

    // Highest latitude the ground track reaches.
    double maxTrackLat = inclinationDeg;
    if (maxTrackLat > 90.0) maxTrackLat = 180.0 - maxTrackLat;

    return std::fabs(obsLatDeg) <= (maxTrackLat + lambdaMaxDeg);
}

std::vector<Pass> predictPasses(Propagator& prop,
                                const Observer& obs,
                                double stdMag,
                                int64_t fromUnix,
                                int horizonHours,
                                YieldFn onYield) {
    std::vector<Pass> passes;
    if (horizonHours <= 0 || !prop.ready()) return passes;

    const int64_t until = fromUnix + static_cast<int64_t>(horizonHours) * 3600LL;

    bool    inPass      = false;
    int64_t enteredAt   = 0;
    int64_t prevT       = fromUnix;
    double  prevEl      = elevationAt(prop, obs, fromUnix);
    double  bestEl      = -999.0;
    int64_t bestT       = 0;
    int     stepCounter = 0;

    for (int64_t t = fromUnix + COARSE_STEP_SEC; t <= until; t += COARSE_STEP_SEC) {
        if (onYield != nullptr && ++stepCounter % YIELD_EVERY_STEPS == 0) onYield();

        const double el = elevationAt(prop, obs, t);

        if (!inPass && prevEl < MIN_ELEVATION_DEG && el >= MIN_ELEVATION_DEG) {
            inPass    = true;
            enteredAt = refineCrossing(prop, obs, prevT, t);
            bestEl    = el;
            bestT     = t;
        } else if (inPass) {
            if (el > bestEl) {
                bestEl = el;
                bestT  = t;
            }
            if (prevEl >= MIN_ELEVATION_DEG && el < MIN_ELEVATION_DEG) {
                Pass p;
                p.riseUnix = enteredAt;
                p.setUnix  = refineCrossing(prop, obs, t, prevT);
                p.maxUnix  = bestT;
                p.maxElDeg = bestEl;

                // Judge visibility at maximum elevation, the brightest moment.
                Vec3 pos;
                if (prop.positionAt(bestT, pos)) {
                    const double jd   = timeutils::julianDate(bestT);
                    const double gmst = timeutils::gmstDegrees(jd);
                    const LookAngles la = look(pos, obs, gmst);
                    const Vec3 sun  = sunEci(jd);
                    const Vec3 site = siteEci(obs, gmst);
                    p.visible = judge(la, pos, sun, site,
                                      sunAltitudeDeg(obs, bestT), stdMag).visible;
                }

                passes.push_back(p);
                inPass = false;
                bestEl = -999.0;
            }
        }

        prevEl = el;
        prevT  = t;
    }

    return passes;
}
