// Display pin integrity diagnostic - not part of the firmware.
//
// Run with the DISPLAY DISCONNECTED. The panel failed and released smoke on its
// VCC, so the question is no longer "what is wrong with the panel" - it is
// whether the ESP32 survived feeding that short, and whether the soldered
// harness is sound enough to trust a replacement panel to.
//
// Everything here is automatic and needs no multimeter. It works by using the
// pin's own input path and internal pulls, which can see things a continuity
// check cannot:
//
//   1  RAIL SHORTS      An internal pull-up should read high on a floating pin.
//                       If it reads low, that line is tied to ground somewhere.
//                       The pull-down case catches the mirror fault, a line
//                       tied to 3.3 V.
//
//   2  PIN-TO-PIN       Drive one line high while every other line is pulled
//                       down, then read them all. Any line that follows is
//                       bridged to the driven one. This is the test that finds
//                       solder bridges, which is exactly the failure a fresh
//                       soldering job invites - and a two-point continuity
//                       check will never stumble across the specific pair.
//
//   3  OUTPUT DRIVER    Drive the line, then immediately release it to a
//                       floating input and read. Pin capacitance holds the
//                       charge for milliseconds, so a working driver reads back
//                       the level it just drove. A driver destroyed by the
//                       short reads back the opposite, or nothing. THIS is what
//                       continuity cannot substitute for: a blown output stage
//                       leaves the wire perfectly continuous.
//
// Phase 4 stays available for a multimeter check if any of the above is
// ambiguous, but it only runs when the BOOT button is held at startup.

#include <Arduino.h>

#include "Pins.h"

namespace {

struct Line { int pin; const char* name; };

// Only the six display control lines. Nothing here touches the flash or PSRAM
// pins (26-37), which would crash the chip.
const Line LINES[] = {
    {PIN_TFT_BL,   "BL  "},
    {PIN_TFT_CS,   "CS  "},
    {PIN_TFT_MOSI, "SDA "},
    {PIN_TFT_SCLK, "SCL "},
    {PIN_TFT_DC,   "DC  "},
    {PIN_TFT_RST,  "RST "},
};
constexpr int N = sizeof(LINES) / sizeof(LINES[0]);

constexpr int BOOT_PIN = 0;

int failures = 0;

void allInputs() {
    for (int i = 0; i < N; ++i) pinMode(LINES[i].pin, INPUT);
}

// --- 1. Shorts to either rail ------------------------------------------
void phaseRails() {
    Serial.println();
    Serial.println("--- 1. SHORTS TO GROUND OR 3V3 ---------------------");

    for (int i = 0; i < N; ++i) {
        const int p = LINES[i].pin;

        pinMode(p, INPUT_PULLUP);
        delay(6);
        const int up = digitalRead(p);

        pinMode(p, INPUT_PULLDOWN);
        delay(6);
        const int down = digitalRead(p);

        pinMode(p, INPUT);

        const bool shortedLow  = (up == LOW);
        const bool shortedHigh = (down == HIGH);

        if (shortedLow) {
            Serial.printf("  %s GPIO%-2d  FAIL - tied to GND\n", LINES[i].name, p);
            failures++;
        } else if (shortedHigh) {
            Serial.printf("  %s GPIO%-2d  FAIL - tied to 3V3\n", LINES[i].name, p);
            failures++;
        } else {
            Serial.printf("  %s GPIO%-2d  ok (floats both ways)\n", LINES[i].name, p);
        }
    }
}

// --- 2. Bridges between lines ------------------------------------------
void phaseBridges() {
    Serial.println();
    Serial.println("--- 2. BRIDGES BETWEEN LINES (solder shorts) -------");

    bool any = false;
    for (int d = 0; d < N; ++d) {
        // Every other line held down, so only a real connection can pull one up.
        for (int i = 0; i < N; ++i) {
            if (i != d) pinMode(LINES[i].pin, INPUT_PULLDOWN);
        }
        pinMode(LINES[d].pin, OUTPUT);
        digitalWrite(LINES[d].pin, HIGH);
        delay(8);

        for (int i = 0; i < N; ++i) {
            if (i == d) continue;
            if (digitalRead(LINES[i].pin) == HIGH) {
                Serial.printf("  FAIL - %s (GPIO%d) bridged to %s (GPIO%d)\n",
                              LINES[d].name, LINES[d].pin,
                              LINES[i].name, LINES[i].pin);
                failures++;
                any = true;
            }
        }
        pinMode(LINES[d].pin, INPUT);
    }
    allInputs();
    if (!any) Serial.println("  ok - no bridges between any pair");
}

// --- 3. Output drivers still alive --------------------------------------
void phaseDrivers() {
    Serial.println();
    Serial.println("--- 3. OUTPUT DRIVERS ------------------------------");

    for (int i = 0; i < N; ++i) {
        const int p = LINES[i].pin;
        bool ok = true;

        // Drive high, release to a floating input, read the held charge.
        pinMode(p, OUTPUT);
        digitalWrite(p, HIGH);
        delayMicroseconds(300);
        pinMode(p, INPUT);
        if (digitalRead(p) != HIGH) ok = false;

        pinMode(p, OUTPUT);
        digitalWrite(p, LOW);
        delayMicroseconds(300);
        pinMode(p, INPUT);
        if (digitalRead(p) != LOW) ok = false;

        pinMode(p, INPUT);

        if (ok) {
            Serial.printf("  %s GPIO%-2d  ok - drives both levels\n", LINES[i].name, p);
        } else {
            Serial.printf("  %s GPIO%-2d  FAIL - cannot drive\n", LINES[i].name, p);
            failures++;
        }
    }
}

// --- 4. Manual multimeter sweep (opt-in) --------------------------------
void phaseMultimeter() {
    Serial.println();
    Serial.println("--- 4. MULTIMETER SWEEP ----------------------------");
    Serial.println("  Black probe on GND. Expect 3.3 V then 0 V.");
    for (int i = 0; i < N; ++i) {
        pinMode(LINES[i].pin, OUTPUT);
        Serial.printf("  %s GPIO%-2d  HIGH\n", LINES[i].name, LINES[i].pin);
        digitalWrite(LINES[i].pin, HIGH);
        delay(4000);
        Serial.printf("  %s GPIO%-2d  LOW\n", LINES[i].name, LINES[i].pin);
        digitalWrite(LINES[i].pin, LOW);
        delay(4000);
        pinMode(LINES[i].pin, INPUT);
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(600);
    pinMode(BOOT_PIN, INPUT_PULLUP);
    allInputs();

    Serial.println();
    Serial.println("====================================================");
    Serial.println(" DISPLAY HARNESS DIAGNOSTIC - run with panel removed");
    Serial.println("====================================================");
    Serial.printf(" heap %u   psram %u\n", ESP.getFreeHeap(), ESP.getFreePsram());

    phaseRails();
    phaseBridges();
    phaseDrivers();

    Serial.println();
    Serial.println("====================================================");
    if (failures == 0) {
        Serial.println(" RESULT: all six lines pass.");
        Serial.println(" The ESP32 side survived. Safe for a new panel.");
    } else {
        Serial.printf(" RESULT: %d failure(s) above - fix before fitting\n", failures);
        Serial.println(" a replacement panel, or it will fail the same way.");
    }
    Serial.println("====================================================");

    if (digitalRead(BOOT_PIN) == LOW) phaseMultimeter();
}

void loop() {
    delay(5000);
}
