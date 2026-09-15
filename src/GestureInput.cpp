#include "GestureInput.h"

#include <Arduino.h>
#include <RevEng_PAJ7620.h>
#include <Wire.h>

#include "Display.h"
#include "Pins.h"
#include "ScopeService.h"

namespace gestureinput {

namespace {

RevEng_PAJ7620 sensor;
bool g_present  = false;
bool g_inverted = false;

// The sensor latches a gesture and holds it until read, so polling faster than
// this buys nothing and just hammers the I2C bus.
constexpr uint32_t POLL_INTERVAL_MS = 60;
uint32_t g_lastPollMs = 0;

// A single swipe often reports twice in quick succession as the hand leaves the
// field of view. Without this, one deliberate swipe toggles the mode and
// immediately toggles it back, which reads as the gesture not working at all.
constexpr uint32_t REPEAT_LOCKOUT_MS = 700;
uint32_t g_lastActionMs = 0;

const char* gestureName(Gesture g) {
    switch (g) {
        case GES_UP:            return "up";
        case GES_DOWN:          return "down";
        case GES_LEFT:          return "left";
        case GES_RIGHT:         return "right";
        case GES_FORWARD:       return "forward";
        case GES_BACKWARD:      return "backward";
        case GES_CLOCKWISE:     return "clockwise";
        case GES_ANTICLOCKWISE: return "anticlockwise";
        case GES_WAVE:          return "wave";
        default:                return "none";
    }
}

Gesture applyInversion(Gesture g) {
    if (!g_inverted) return g;
    switch (g) {
        case GES_LEFT:  return GES_RIGHT;
        case GES_RIGHT: return GES_LEFT;
        case GES_UP:    return GES_DOWN;
        case GES_DOWN:  return GES_UP;
        default:        return g;
    }
}

}  // namespace

bool begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // INT is wired but not used as an interrupt source yet - polling one
    // register every 60 ms is already cheap, and an ISR here would have to
    // reach into ScopeService from interrupt context. Configured as an input so
    // the line is never left floating against the sensor's open-drain output.
    pinMode(PIN_GESTURE_INT, INPUT_PULLUP);

    if (sensor.begin(&Wire) == 0) {
        Serial.println("[gesture] PAJ7620 not responding on I2C - input disabled");
        g_present = false;
        return false;
    }

    sensor.setGestureMode();
    g_present = true;
    Serial.printf("[gesture] PAJ7620 ready on SDA=%d SCL=%d\n",
                  PIN_I2C_SDA, PIN_I2C_SCL);
    return true;
}

bool present() { return g_present; }

void setInverted(bool inverted) { g_inverted = inverted; }

void loop() {
    if (!g_present) return;
    if (millis() - g_lastPollMs < POLL_INTERVAL_MS) return;
    g_lastPollMs = millis();

    const Gesture raw = sensor.readGesture();
    if (raw == GES_NONE) return;

    if (millis() - g_lastActionMs < REPEAT_LOCKOUT_MS) return;

    const Gesture g = applyInversion(raw);
    switch (g) {
        case GES_RIGHT: scope::setMode(ScopeMode::Neo); break;
        case GES_LEFT:  scope::setMode(ScopeMode::Sky); break;
        case GES_UP:    display::nudgeBrightness(+1);   break;
        case GES_DOWN:  display::nudgeBrightness(-1);   break;
        case GES_WAVE:  display::wake();                break;
        default:
            // Recognised but unbound. Logged rather than silently dropped, so
            // "the sensor does nothing" and "the sensor read something I did
            // not bind" are distinguishable from the serial console.
            Serial.printf("[gesture] %s (unbound)\n", gestureName(g));
            return;
    }

    g_lastActionMs = millis();
    Serial.printf("[gesture] %s\n", gestureName(g));
}

}  // namespace gestureinput
