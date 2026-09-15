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
// enable pin, and nothing ever lit. M6 replaces it with two external WS2812s,
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

// Walks the strip once at boot: pixel 0, then pixel 1, then both, then dark.
//
// Worth the 700 ms because of what the quiet state looks like. A correctly
// working bar is *off* nearly all the time - it only lights for an imminent
// visible pass - so "wired correctly" and "not wired at all" are the same
// picture for hours at a stretch. That ambiguity is precisely what made the
// onboard RGB take three rounds of probing to call dead.
//
// Lighting them in order also answers the one question mounting depends on:
// which physical end is pixel 0, and therefore which way the countdown fills.
static void selfTest() {
    if (!ws2812::ready()) return;

    LedColor px[LED_COUNT];
    const LedColor on{LED_MAX_BRIGHTNESS, 0, 0};

    for (int lit = 1; lit <= LED_COUNT; ++lit) {
        for (int i = 0; i < LED_COUNT; ++i) px[i] = (i < lit) ? on : LedColor{};
        ws2812::show(px, LED_COUNT);
        delay(220);
    }

    for (int i = 0; i < LED_COUNT; ++i) px[i] = LedColor{};
    ws2812::show(px, LED_COUNT);
    Serial.println("[led] self-test done (pixel 0 lit first)");
}

void begin() {
    ws2812::begin(STATUS_LED_PIN, LED_COUNT);
    selfTest();

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
