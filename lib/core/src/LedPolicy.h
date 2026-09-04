#pragma once

#include <cstdint>
#include "Snapshot.h"

struct LedColor {
    uint8_t r = 0, g = 0, b = 0;
};

// A night instrument. A WS2812 at full scale is blinding in a dark room and
// destroys the dark adaptation the whole red palette exists to protect.
constexpr uint8_t LED_MAX_BRIGHTNESS = 64;

// Only alert about a pass this close. Further out is not actionable.
constexpr int64_t ALERT_WINDOW_SEC = 600;

// Colour constants: per-state channel maxima used in ledFor().
// Each must not exceed LED_MAX_BRIGHTNESS to preserve the night-adaptation guarantee.
namespace ColorChannels {
    // Setup states (NoTime, NoLocation): amber-ish, warm
    constexpr double SETUP_RED   = LED_MAX_BRIGHTNESS;           // 64
    constexpr double SETUP_GREEN = LED_MAX_BRIGHTNESS * 0.55;    // ~35.2
    constexpr double SETUP_BLUE  = 0.0;

    // Visible right now: cool colour (#d8f4ff)
    constexpr double VISIBLE_RED   = LED_MAX_BRIGHTNESS * 0.55;  // ~35.2
    constexpr double VISIBLE_GREEN = LED_MAX_BRIGHTNESS * 0.85;  // ~54.4
    constexpr double VISIBLE_BLUE  = LED_MAX_BRIGHTNESS;         // 64

    // Imminent pass alert: red
    constexpr double ALERT_RED   = LED_MAX_BRIGHTNESS;           // 64
    constexpr double ALERT_GREEN = 0.0;
    constexpr double ALERT_BLUE  = 0.0;
}

// Pure function of the snapshot and a millisecond phase clock, so every rule is
// natively testable and the firmware only writes what it is handed.
//
// Priority: setup needed > something visible right now > imminent visible pass > dark.
LedColor ledFor(const Snapshot& s, uint32_t phaseMs);
