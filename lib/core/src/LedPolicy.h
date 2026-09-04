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

// Pure function of the snapshot and a millisecond phase clock, so every rule is
// natively testable and the firmware only writes what it is handed.
//
// Priority: setup needed > something visible right now > imminent visible pass > dark.
LedColor ledFor(const Snapshot& s, uint32_t phaseMs);
