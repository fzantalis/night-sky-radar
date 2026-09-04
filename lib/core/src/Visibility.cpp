#include "Visibility.h"

#include "Eclipse.h"
#include "Magnitude.h"

const char* visReasonName(VisReason r) {
    switch (r) {
        case VisReason::Visible:      return "visible";
        case VisReason::BelowHorizon: return "below_horizon";
        case VisReason::Daylight:     return "daylight";
        case VisReason::Eclipsed:     return "eclipsed";
        case VisReason::TooDim:       return "too_dim";
    }
    return "below_horizon";
}

Verdict judge(const LookAngles& la,
              const Vec3& satEci,
              const Vec3& sunEci,
              const Vec3& siteEci,
              double sunAltDeg,
              double stdMag) {
    Verdict v;

    if (la.elDeg <= MIN_ELEVATION_DEG) {
        v.reason = VisReason::BelowHorizon;
        return v;
    }

    if (sunAltDeg >= MAX_SUN_ALT_DEG) {
        v.reason = VisReason::Daylight;
        return v;
    }

    if (!isSunlit(satEci, sunEci)) {
        v.reason = VisReason::Eclipsed;
        return v;
    }

    // Brightness is computed last because it is the only expensive condition,
    // and it is reported even on the TooDim path so the renderer can still size
    // the blip.
    const double phi = phaseAngleRad(satEci, sunEci, siteEci);
    v.magnitude = apparentMagnitude(stdMag, la.rangeKm, phi);

    if (v.magnitude > FAINTEST_VISIBLE_MAG) {
        v.reason = VisReason::TooDim;
        return v;
    }

    v.visible = true;
    v.reason  = VisReason::Visible;
    return v;
}
