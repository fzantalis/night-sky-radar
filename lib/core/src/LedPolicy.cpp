#include "LedPolicy.h"

#include <algorithm>
#include <cstdint>

namespace {

// Continuous triangle breathe: rises from `floorFrac` to 1.0 across the first
// half of the period, then falls back to `floorFrac` across the second half.
// Never touches zero for an "active" state - phaseMs == 0 must still read as
// lit, not off, so a caller can't mistake "just started breathing" for "off".
double breathe(uint32_t phaseMs, uint32_t periodMs, double floorFrac) {
    if (periodMs == 0) return 1.0;
    const uint32_t t    = phaseMs % periodMs;
    const double   frac = static_cast<double>(t) / static_cast<double>(periodMs);
    const double   tri  = (frac < 0.5) ? (frac * 2.0) : (2.0 - frac * 2.0);
    return floorFrac + (1.0 - floorFrac) * tri;
}

uint8_t scale(double channelMax, double brightness01) {
    double v = channelMax * brightness01;
    if (v < 0.0) v = 0.0;
    if (v > LED_MAX_BRIGHTNESS) v = LED_MAX_BRIGHTNESS;
    return static_cast<uint8_t>(v + 0.5);
}

constexpr double SETUP_PERIOD_MS  = 3000.0;
constexpr double VISIBLE_PERIOD_MS = 4000.0;
constexpr double ALERT_PERIOD_NEAR_MS = 400.0;   // startsIn == 0
constexpr double ALERT_PERIOD_FAR_MS  = 2000.0;  // startsIn == ALERT_WINDOW_SEC

// --- State classification -------------------------------------------------
//
// Factored out so ledFor() and ledStripFor() cannot drift apart about which
// state the instrument is in. They differ only in how many pixels they paint.

bool needsSetup(const Snapshot& s) {
    return s.status == ScopeStatus::NoTime || s.status == ScopeStatus::NoLocation;
}

bool anyVisibleNow(const Snapshot& s) {
    for (const Blip& b : s.blips) {
        if (b.visible) return true;
    }
    return false;
}

// Soonest visible event inside the alert window - only a naked-eye pass is
// worth walking outside for. Returns false when there is none.
bool soonestAlert(const Snapshot& s, int64_t& out) {
    bool    found   = false;
    int64_t soonest = ALERT_WINDOW_SEC;
    for (const Event& e : s.events) {
        if (!e.visible) continue;
        if (e.startsIn < 0 || e.startsIn > ALERT_WINDOW_SEC) continue;
        if (!found || e.startsIn < soonest) {
            found   = true;
            soonest = e.startsIn;
        }
    }
    if (found) out = soonest;
    return found;
}

LedColor setupColour(uint32_t phaseMs) {
    const double br = breathe(phaseMs, static_cast<uint32_t>(SETUP_PERIOD_MS), 0.15);
    LedColor c;
    c.r = scale(ColorChannels::SETUP_RED, br);
    c.g = scale(ColorChannels::SETUP_GREEN, br);
    c.b = scale(ColorChannels::SETUP_BLUE, br);
    return c;
}

LedColor visibleColour(uint32_t phaseMs) {
    const double br = breathe(phaseMs, static_cast<uint32_t>(VISIBLE_PERIOD_MS), 0.15);
    LedColor c;
    c.r = scale(ColorChannels::VISIBLE_RED, br);
    c.g = scale(ColorChannels::VISIBLE_GREEN, br);
    c.b = scale(ColorChannels::VISIBLE_BLUE, br);
    return c;
}

LedColor alertColour(uint32_t phaseMs, int64_t soonest) {
    // Urgency: the pulse period shrinks linearly from ~2000ms at the window
    // edge down to ~400ms as startsIn approaches zero, so the pass reads as
    // more imminent without needing the dial.
    const double frac = static_cast<double>(soonest) / static_cast<double>(ALERT_WINDOW_SEC);
    const double periodMs = ALERT_PERIOD_NEAR_MS + frac * (ALERT_PERIOD_FAR_MS - ALERT_PERIOD_NEAR_MS);
    const double br = breathe(phaseMs, static_cast<uint32_t>(periodMs), 0.15);
    LedColor c;
    c.r = scale(ColorChannels::ALERT_RED, br);
    c.g = scale(ColorChannels::ALERT_GREEN, br);
    c.b = scale(ColorChannels::ALERT_BLUE, br);
    return c;
}

}  // namespace

int countdownPixels(int64_t startsIn) {
    if (startsIn < 0 || startsIn > ALERT_WINDOW_SEC) return 0;
    if (startsIn > ALERT_STAGE_2_SEC) return 1;
    if (startsIn > ALERT_STAGE_3_SEC) return 2;
    return LED_COUNT;
}

LedColor ledFor(const Snapshot& s, uint32_t phaseMs) {
    // Setup states outrank everything else - nothing else can even be
    // computed correctly without a clock and a location.
    if (needsSetup(s)) return setupColour(phaseMs);

    // Something is up right now: the reserved cool "visible" colour outranks
    // an imminent pass, since "up now" is strictly more actionable than
    // "coming soon".
    if (anyVisibleNow(s)) return visibleColour(phaseMs);

    int64_t soonest = 0;
    if (soonestAlert(s, soonest)) return alertColour(phaseMs, soonest);

    // Quiet sky: fully off, not dim. This sits in a dark room.
    return LedColor{};
}

LedStrip ledStripFor(const Snapshot& s, uint32_t phaseMs) {
    LedStrip out;   // every pixel defaults to off

    // Setup and "visible right now" describe the whole instrument, not a time
    // remaining. Filling only part of the bar for them would read as a
    // countdown that is not counting down to anything, so the bar is uniform.
    if (needsSetup(s)) {
        const LedColor c = setupColour(phaseMs);
        for (int i = 0; i < LED_COUNT; ++i) out.px[i] = c;
        return out;
    }

    if (anyVisibleNow(s)) {
        const LedColor c = visibleColour(phaseMs);
        for (int i = 0; i < LED_COUNT; ++i) out.px[i] = c;
        return out;
    }

    int64_t soonest = 0;
    if (soonestAlert(s, soonest)) {
        const LedColor c = alertColour(phaseMs, soonest);
        const int lit = countdownPixels(soonest);
        // Fill from pixel 0, which is the end nearest the data input - so the
        // bar always grows from the same physical end no matter how the strip
        // is mounted.
        for (int i = 0; i < lit && i < LED_COUNT; ++i) out.px[i] = c;
        return out;
    }

    return out;   // quiet sky: all off
}
