#include "StatusLed.h"

#include <Arduino.h>

#include "LedPolicy.h"

// Which GPIO the status LED sits on.
//
// Defaults to the board's own RGB_BUILTIN, which on the esp32s3 variant is 97
// and resolves to GPIO48. On this particular DevKitC-1 clone that lights
// nothing: tools/ledprobe swept every safely drivable pin as a plain LED of
// either polarity and as a WS2812, including a pass that held every other pin
// high in case the LED's power is gated behind an enable pin, and no pin ever
// lit the onboard part. The conclusion is that it is not reachable from
// software on this board.
//
// The policy layer (LedPolicy.cpp) is unaffected by that and stays fully
// tested, so wiring an external WS2812 to any free GPIO needs no code change -
// just build with -DSTATUS_LED_PIN=<gpio>. Data to the GPIO, 5V or 3V3 to VCC,
// ground to ground.
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN RGB_BUILTIN
#endif

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
    Serial.printf("[led] status LED on pin %d\n", static_cast<int>(STATUS_LED_PIN));

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

    neopixelWrite(STATUS_LED_PIN, c.r, c.g, c.b);
    lastColor   = c;
    everWritten = true;
}

}  // namespace statusled
