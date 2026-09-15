#pragma once

#include <cstdint>

#include "LedPolicy.h"   // LedColor

// A minimal WS2812 driver for the short status strip.
//
// The Arduino core ships neopixelWrite(), but it can only ever write a single
// pixel: it builds exactly 24 RMT symbols per call and has no notion of a
// chain. It also caches its RMT channel in a function-local static and only
// initialises it on the first call, so the pin argument is ignored on every
// later call. Neither limitation is fixable from outside, hence this.
//
// Timing matches the core's own esp32-hal-rgb-led.c at a 100 ns tick:
// T0H 0.4us / T0L 0.8us, T1H 0.8us / T1L 0.4us.
namespace ws2812 {

// Claims an RMT channel on `pin`. Returns false if the channel could not be
// allocated, in which case show() is a safe no-op - a missing status LED must
// never take the instrument down with it.
bool begin(int pin, int count);

bool ready();

// Pushes `count` pixels, nearest the data input first. Colours are taken as
// RGB and reordered to the WS2812's GRB wire order here, so no caller has to
// know about that.
void show(const LedColor* px, int count);

}  // namespace ws2812
