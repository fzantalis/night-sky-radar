#pragma once

// Single source of truth for how this instrument is wired (M6).
//
// Board: ESP32-S3-DevKitC-1 clone, ESP32-S3-WROOM-1 N16R8 module.
//
// PINS THAT MUST NEVER BE USED ON THIS MODULE:
//   26 - 32   SPI flash
//   33 - 37   octal PSRAM. Broken out on the header and therefore a trap -
//             35/36/37 sit in the middle of the pin row and driving any of
//             them crashes the chip.
//   19, 20    native USB D-/D+
//   43, 44    UART0, the serial console this project debugs over
//   0         BOOT button / strapping
//   45, 46    strapping pins, sampled at reset
//
// Everything below is drawn from what is left.

// --- Display: GC9B72, 2.1", 360x360, 4-wire SPI -------------------------
//
// 3.3 V ONLY. The panel's reference driver is explicit that 5 V on VCC
// destroys it, and there is no regulator on the breakout.
//
// CS/MOSI/SCLK are the ESP32-S3's native FSPI pins, which keeps the transfer
// on the dedicated IO mux instead of routing through the GPIO matrix, so the
// panel can be clocked as fast as the wiring allows. Keep these leads short -
// the panel is good for roughly 20 MHz on jumper wires, less if they are long.
//
// SDO (read-back) and TE (frame sync) are deliberately left unconnected: this
// renderer never reads the panel, and never needs to sync to its refresh.
constexpr int PIN_TFT_SCLK = 12;   // panel "SCL"
constexpr int PIN_TFT_MOSI = 11;   // panel "SDA"
constexpr int PIN_TFT_CS   = 10;
constexpr int PIN_TFT_DC   = 13;
constexpr int PIN_TFT_RST  = 14;
constexpr int PIN_TFT_BL   = 9;    // backlight, on a GPIO so it can be dimmed

constexpr int TFT_WIDTH_PX  = 360;
constexpr int TFT_HEIGHT_PX = 360;

// --- Gesture sensor: PAJ7620U2 over I2C ---------------------------------
//
// 3.3 V ONLY - a 2.8-3.3 V part, and the bare purple breakouts carry no
// regulator. I2C address 0x73. INT is active low.
//
// Not the Arduino default I2C pins (8/9): GPIO 9 is the panel backlight, and
// keeping the display's block contiguous matters more than the default, since
// Wire takes explicit pins anyway.
constexpr int PIN_I2C_SDA     = 17;
constexpr int PIN_I2C_SCL     = 18;
constexpr int PIN_GESTURE_INT = 16;

// --- Status LEDs: 2x WS2812 ---------------------------------------------
//
// Powered from 5 V, driven with 3.3 V logic. That is marginally below the
// WS2812's nominal 0.7*VDD threshold and is the first thing to suspect if the
// pixels flicker or show wrong colours; a signal diode in series with the
// strip's +5 V drops it to ~4.3 V and fixes it.
//
// Pixel 0 is the end nearest the data input - the countdown bar always fills
// from there (see LedPolicy.cpp).
constexpr int PIN_LED_DATA = 15;
