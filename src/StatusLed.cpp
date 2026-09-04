#include "StatusLed.h"

#include <Arduino.h>

#include "LedPolicy.h"

namespace statusled {

namespace {

// ~20 Hz. Fast enough that the fastest breathe period (~400ms near an
// imminent pass) still looks smooth, cheap enough to call unconditionally
// from loop().
constexpr uint32_t WRITE_INTERVAL_MS = 50;

uint32_t lastWriteMs = 0;
LedColor lastColor{};
bool     everWritten = false;

}  // namespace

void begin() {
    // Force the first update() to actually write, even if ledFor() happens to
    // return {0,0,0} (quiet sky) on boot - neopixelWrite() itself defaults the
    // pixel off, but this keeps begin()/update() free of any assumption about
    // hardware reset state.
    lastWriteMs  = millis() - WRITE_INTERVAL_MS;
    everWritten  = false;
}

void update(const Snapshot& s) {
    // Rollover-safe, same pattern used elsewhere in this codebase.
    if (millis() - lastWriteMs < WRITE_INTERVAL_MS) return;
    lastWriteMs = millis();

    const LedColor c = ledFor(s, millis());

    // No policy here - ledFor() decided everything. This is purely "did the
    // answer change" so an unchanged colour doesn't retrigger the RMT
    // transaction 20 times a second for nothing.
    if (everWritten && c.r == lastColor.r && c.g == lastColor.g && c.b == lastColor.b) {
        return;
    }

    neopixelWrite(RGB_BUILTIN, c.r, c.g, c.b);
    lastColor   = c;
    everWritten = true;
}

}  // namespace statusled
