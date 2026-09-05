// Onboard LED pin sweep - diagnostic only, not part of the firmware.
//
// Round 1 drove GPIO48 (Arduino's RGB_BUILTIN for the esp32s3 variant, and the
// DevKitC-1 v1.0 pin) and GPIO38 (the v1.1 pin) on separate RMT channels, each
// with its own colour. Both initialised cleanly and neither lit anything, which
// rules out the v1.0/v1.1 question and means the pin has to be found, not
// guessed.
//
// Round 2 swept every safely drivable GPIO, trying each as a plain LED driven
// high, a plain LED driven low, and a WS2812. Still nothing reported.
//
// Round 3 - this one - adds the hypothesis that single-pin sweeping structurally
// cannot catch: several ESP32-S3 boards gate the RGB LED's *power* through a
// separate enable pin (Adafruit's PIN_NEOPIXEL_POWER is the well-known case).
// On such a board the LED is unpowered for the entire sweep, so no amount of
// driving its data pin can ever light it. Pass B therefore holds every other
// candidate pin high - whichever one is the enable, it is now on - while
// clocking WS2812 data into the pin under test.
//
// Reporting is by button, not by clock: correlating a serial timeline against
// something you are watching is error-prone, so the board reads GPIO0 (the BOOT
// button) and names the pass, pin and drive mode that were live when pressed.
//
// Excluded from the sweep, deliberately:
//   0          BOOT button - this probe's input
//   19, 20     native USB D-/D+
//   26 - 32    SPI flash
//   33 - 37    octal PSRAM on the N16R8 module; driving these crashes the chip
//   43, 44     UART0 TX/RX - driving these kills the serial report

#include <Arduino.h>

namespace {

const int CANDIDATES[] = {
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
    16, 17, 18, 21, 38, 39, 40, 41, 42, 45, 46, 47, 48,
};
constexpr int CANDIDATE_COUNT = sizeof(CANDIDATES) / sizeof(CANDIDATES[0]);

constexpr int BOOT_PIN = 0;
constexpr uint8_t WS_LEVEL = 120;  // bright white, diagnostic legibility

bool found = false;
int  foundPin   = -1;
const char* foundPhase = "";

bool buttonPressed() { return digitalRead(BOOT_PIN) == LOW; }

// Waits `ms`, returning early (true) if the BOOT button goes down.
bool holdOrPress(uint32_t ms) {
    const uint32_t until = millis() + ms;
    while (static_cast<int32_t>(until - millis()) > 0) {
        if (buttonPressed()) return true;
        delay(5);
    }
    return false;
}

// WS2812 bit encoding at a 100 ns tick, matching the core's own
// esp32-hal-rgb-led.c: T0H 0.4us / T0L 0.8us, T1H 0.8us / T1L 0.4us.
void sendPixel(rmt_obj_t* ch, uint8_t r, uint8_t g, uint8_t b) {
    rmt_data_t bits[24];
    const int colour[3] = {g, r, b};  // WS2812 wire order is GRB
    int i = 0;
    for (int c = 0; c < 3; ++c) {
        for (int bit = 0; bit < 8; ++bit) {
            const bool high = colour[c] & (1 << (7 - bit));
            bits[i].level0    = 1;
            bits[i].duration0 = high ? 8 : 4;
            bits[i].level1    = 0;
            bits[i].duration1 = high ? 4 : 8;
            ++i;
        }
    }
    rmtWrite(ch, bits, 24);
}

void record(int pin, const char* phase) {
    found      = true;
    foundPin   = pin;
    foundPhase = phase;
    Serial.println();
    Serial.println("=======================================");
    Serial.printf("  FOUND IT: GPIO%d  (%s)\n", pin, phase);
    Serial.println("=======================================");
    Serial.println();
}

void releaseAll() {
    for (int i = 0; i < CANDIDATE_COUNT; ++i) pinMode(CANDIDATES[i], INPUT);
}

// Pass A: each pin alone, in all three drive modes.
bool tryPinAlone(int pin) {
    Serial.printf("[A] GPIO%-2d ", pin);

    Serial.print("high ");
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    if (holdOrPress(450)) { record(pin, "pass A: plain LED, active high"); return true; }

    Serial.print("low ");
    digitalWrite(pin, LOW);
    if (holdOrPress(450)) { record(pin, "pass A: plain LED, active low"); return true; }

    Serial.print("ws2812 ");
    rmt_obj_t* ch = rmtInit(pin, RMT_TX_MODE, RMT_MEM_64);
    if (ch != nullptr) {
        rmtSetTick(ch, 100);
        sendPixel(ch, WS_LEVEL, WS_LEVEL, WS_LEVEL);
        const bool hit = holdOrPress(650);
        sendPixel(ch, 0, 0, 0);
        delay(5);
        rmtDeinit(ch);
        if (hit) { record(pin, "pass A: WS2812"); return true; }
    } else {
        Serial.print("(rmtInit failed) ");
    }

    pinMode(pin, INPUT);
    Serial.println("- dark");
    return false;
}

// Pass B: every other candidate pin held high as a possible power enable,
// WS2812 data on the pin under test.
bool tryPinPowered(int pin) {
    Serial.printf("[B] GPIO%-2d ws2812 + all others high ", pin);

    for (int i = 0; i < CANDIDATE_COUNT; ++i) {
        const int other = CANDIDATES[i];
        if (other == pin) continue;
        pinMode(other, OUTPUT);
        digitalWrite(other, HIGH);
    }

    bool hit = false;
    rmt_obj_t* ch = rmtInit(pin, RMT_TX_MODE, RMT_MEM_64);
    if (ch != nullptr) {
        rmtSetTick(ch, 100);
        sendPixel(ch, WS_LEVEL, WS_LEVEL, WS_LEVEL);
        hit = holdOrPress(700);
        sendPixel(ch, 0, 0, 0);
        delay(5);
        rmtDeinit(ch);
    } else {
        Serial.print("(rmtInit failed) ");
    }

    releaseAll();
    if (hit) { record(pin, "pass B: WS2812 with another pin supplying power"); return true; }
    Serial.println("- dark");
    return false;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(400);
    pinMode(BOOT_PIN, INPUT_PULLUP);
    Serial.println("\n[sweep] onboard LED pin sweep, round 3");
    Serial.printf("[sweep] %d candidate pins\n", CANDIDATE_COUNT);
    Serial.println("[sweep] pass A: each pin alone (high / low / WS2812)");
    Serial.println("[sweep] pass B: WS2812 per pin, every other pin held high as a power enable");
    Serial.println("[sweep] PRESS THE BOOT BUTTON the moment you see the LED light");
}

void loop() {
    if (found) {
        Serial.printf("[sweep] holding result: GPIO%d (%s) - reset the board to sweep again\n",
                      foundPin, foundPhase);
        delay(3000);
        return;
    }

    Serial.println("[sweep] --- pass A start (each pin alone) ---");
    for (int i = 0; i < CANDIDATE_COUNT && !found; ++i) {
        if (tryPinAlone(CANDIDATES[i])) return;
    }

    Serial.println("[sweep] --- pass B start (with power enable held high) ---");
    for (int i = 0; i < CANDIDATE_COUNT && !found; ++i) {
        if (tryPinPowered(CANDIDATES[i])) return;
    }

    releaseAll();
    Serial.println("[sweep] --- both passes complete, nothing reported ---");
    delay(1500);
}
