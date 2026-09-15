#pragma once

#include <cstdint>

#include "Snapshot.h"

// M6 - the GC9B72 panel renderer.
//
// This is a dumb polar plotter and nothing else, which is the whole point of
// design spec section 3.1: the core already computed every (r, theta), so this
// file contains no orbital geometry, no projection and no visibility rules. It
// is the same drawing logic as data/radar.js, at 360x360 instead of 240x240,
// fed from the identical snapshot - which is why the panel and the web view
// cannot drift apart.
namespace display {

// Returns false if the panel does not initialise. Not fatal: the instrument
// keeps running headless and stays usable over the web UI, which is also what
// an unplugged ribbon looks like.
bool begin();

bool present();

// Call from loop(). Rate-limits internally.
void loop(const Snapshot& s);

// Backlight, 0-255. A night instrument, so the usable range is deliberately
// low at the bottom end.
void    setBrightness(uint8_t level);
uint8_t brightness();

// Steps through a small ladder of preset levels rather than adding a fixed
// amount, so one swipe is always a visible change at both ends of the range.
void nudgeBrightness(int steps);

// Restores the backlight after the idle dim, and restarts the idle timer.
void wake();

}  // namespace display
