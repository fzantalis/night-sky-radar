#pragma once

#include "Topocentric.h"
#include "Vec3.h"

// Spec section 5 thresholds. All four conditions must hold.
constexpr double MIN_ELEVATION_DEG    = 10.0;   // atmospheric extinction floor
constexpr double MAX_SUN_ALT_DEG      = -6.0;   // end of civil twilight
constexpr double FAINTEST_VISIBLE_MAG = 4.5;    // naked eye, suburban sky

enum class VisReason {
    Visible,
    BelowHorizon,
    Daylight,
    Eclipsed,
    TooDim
};

struct Verdict {
    bool      visible   = false;
    double    magnitude = 99.0;
    VisReason reason    = VisReason::BelowHorizon;
};

// Conditions are evaluated in a fixed order - horizon, then darkness, then
// illumination, then brightness - so the reason is deterministic.
Verdict judge(const LookAngles& la,
              const Vec3& satEci,
              const Vec3& sunEci,
              const Vec3& siteEci,
              double sunAltDeg,
              double stdMag);

const char* visReasonName(VisReason r);
