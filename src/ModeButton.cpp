#include "ModeButton.h"

#include <Arduino.h>

#include "ScopeService.h"

namespace modebutton {

namespace {

constexpr int      PIN            = 0;      // BOOT
constexpr uint32_t DEBOUNCE_MS    = 40;
constexpr uint32_t LONG_PRESS_MS  = 1500;

bool     lastRaw       = true;   // pull-up: true means released
bool     stable        = true;
uint32_t lastChangeMs  = 0;
uint32_t pressedAtMs   = 0;

}  // namespace

void begin() {
    pinMode(PIN, INPUT_PULLUP);
    lastRaw      = digitalRead(PIN) != LOW;
    stable       = lastRaw;
    lastChangeMs = millis();
}

void update() {
    const bool raw = digitalRead(PIN) != LOW;   // true = released

    if (raw != lastRaw) {
        lastRaw      = raw;
        lastChangeMs = millis();
        return;
    }

    // Rollover-safe, matching the pattern used throughout this codebase.
    if (millis() - lastChangeMs < DEBOUNCE_MS) return;
    if (raw == stable) return;

    stable = raw;

    if (!stable) {                    // just went down
        pressedAtMs = millis();
        return;
    }

    // Released. Fire on release rather than on press so a long hold - which is
    // how the board is put into download mode - does not also flip the mode on
    // the way there.
    const uint32_t held = millis() - pressedAtMs;
    if (held < LONG_PRESS_MS) {
        scope::toggleMode();
        Serial.printf("[mode] BOOT pressed - now %s\n",
                      scope::mode() == ScopeMode::Neo ? "NEO" : "SKY");
    }
}

}  // namespace modebutton
