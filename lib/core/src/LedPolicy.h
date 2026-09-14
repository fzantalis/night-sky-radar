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

// M6: three WS2812s on one data line, used as a countdown bar rather than as
// three independent lamps. How many pixels are lit says how close the next
// visible pass is, so the instrument is readable across a dark room without
// reading anything - which one pixel could not do.
constexpr int LED_COUNT = 3;

// Bar stages, in seconds before the pass starts:
//   > 300      one pixel     "something is coming"
//   300 .. 60  two pixels    "get your shoes on"
//   < 60       three pixels  "go outside now"
constexpr int64_t ALERT_STAGE_2_SEC = 300;
constexpr int64_t ALERT_STAGE_3_SEC = 60;

struct LedStrip {
    LedColor px[LED_COUNT];
};

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

// The whole strip.
//
// The countdown only applies to the "imminent pass" state. Setup states and
// "something is visible right now" describe the whole instrument rather than a
// time remaining, so there the entire bar carries one colour - there is nothing
// to count down to.
LedStrip ledStripFor(const Snapshot& s, uint32_t phaseMs);

// How many pixels the bar lights for a pass `startsIn` seconds away. Zero when
// the pass is outside the alert window or already begun. Exposed because it is
// the entire semantic of the bar, and deserves to be tested as such.
int countdownPixels(int64_t startsIn);
