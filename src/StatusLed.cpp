#include "StatusLed.h"

#include <Arduino.h>

#include "LedPolicy.h"
#include "Ws2812.h"

// Which GPIO the status strip sits on.
//
// This was RGB_BUILTIN through M5. The onboard part on this DevKitC-1 clone
// could not be lit from software at all: tools/ledprobe swept every safely
// drivable pin as a plain LED of either polarity and as a WS2812, including a
// pass that held every other pin high in case its power was gated behind an
// enable pin, and nothing ever lit. M6 replaces it with three external WS2812s,
// which also buys the countdown bar - something one pixel could never show.
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 15
#endif

namespace statusled {

namespace {

// ~20 Hz. Fast enough that the fastest breathe period (~400ms near an
// imminent pass) still looks smooth, cheap enough to call unconditionally
// from loop().
constexpr uint32_t WRITE_INTERVAL_MS = 50;

uint32_t lastWriteMs = 0;
LedStrip lastStrip{};
bool     everWritten = false;

bool sameStrip(const LedStrip& a, const LedStrip& b) {
    for (int i = 0; i < LED_COUNT; ++i) {
        if (a.px[i].r != b.px[i].r) return false;
        if (a.px[i].g != b.px[i].g) return false;
        if (a.px[i].b != b.px[i].b) return false;
    }
    return true;
}

}  // namespace

void begin() {
    ws2812::begin(STATUS_LED_PIN, LED_COUNT);

    // Force the first update() to actually write, even if ledStripFor() happens
    // to return an all-dark strip (quiet sky) on boot.
    lastWriteMs = millis() - WRITE_INTERVAL_MS;
    everWritten = false;
}

void update(const Snapshot& s) {
    // Rollover-safe, same pattern used elsewhere in this codebase.
    if (millis() - lastWriteMs < WRITE_INTERVAL_MS) return;
    lastWriteMs = millis();

    const LedStrip strip = ledStripFor(s, millis());

    // No policy here - ledStripFor() decided everything. This is purely "did
    // the answer change" so an unchanged strip doesn't retrigger the RMT
    // transaction 20 times a second for nothing.
    if (everWritten && sameStrip(strip, lastStrip)) return;

    ws2812::show(strip.px, LED_COUNT);
    lastStrip   = strip;
    everWritten = true;
}

}  // namespace statusled
