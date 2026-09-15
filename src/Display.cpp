#define LGFX_USE_V1
#include "Display.h"

#include <Arduino.h>
#include <LovyanGFX.hpp>

#include <cmath>
#include <cstdio>

#include "Pins.h"

namespace display {

namespace {

// --- Panel wiring ---------------------------------------------------------

class SkyPanel : public lgfx::LGFX_Device {
    lgfx::Panel_GC9B72 _panel;
    lgfx::Bus_SPI      _bus;
    lgfx::Light_PWM    _light;

public:
    SkyPanel() {
        {
            auto cfg = _bus.config();
            cfg.spi_host   = SPI2_HOST;      // FSPI, whose native pins we use
            cfg.spi_mode   = 0;              // GC9B72 is mode 0
            // The panel's reference driver reports reliable operation to about
            // 20 MHz on jumper leads. Starting there rather than at the bus
            // maximum: a too-fast clock does not fail cleanly, it produces
            // intermittently corrupt pixels that look like a rendering bug.
            cfg.freq_write = 20000000;
            cfg.freq_read  = 8000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk   = PIN_TFT_SCLK;
            cfg.pin_mosi   = PIN_TFT_MOSI;
            cfg.pin_miso   = -1;             // SDO deliberately unconnected
            cfg.pin_dc     = PIN_TFT_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs   = PIN_TFT_CS;
            cfg.pin_rst  = PIN_TFT_RST;
            cfg.pin_busy = -1;
            cfg.panel_width  = TFT_WIDTH_PX;
            cfg.panel_height = TFT_HEIGHT_PX;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.readable = false;            // no MISO wired
            // Both of these were wrong on the first bring-up, and the way they
            // were wrong is worth recording because one symptom identified two
            // faults.
            //
            // The panel came up with a white field and brownish-yellow lines.
            // A white background can only mean inversion is being applied when
            // it should not - the palette's background is #060305, and nothing
            // else turns that white. But inversion alone does not explain the
            // lines: the chrome colour #c9503c inverts to #36afc3, a pale cyan,
            // not brown. #c3af36 is that same cyan with its red and blue
            // channels exchanged, so the colour order was reversed as well.
            // Hence both flags flip, not just the obvious one.
            cfg.invert   = false;
            cfg.rgb_order = true;            // this panel is RGB, not BGR
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl = PIN_TFT_BL;
            cfg.invert = false;
            cfg.freq   = 12000;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        setPanel(&_panel);
    }
};

SkyPanel      lcd;
lgfx::LGFX_Sprite canvas(&lcd);
bool g_present = false;

// --- Geometry -------------------------------------------------------------

constexpr int CX = TFT_WIDTH_PX / 2;
constexpr int CY = TFT_HEIGHT_PX / 2;
constexpr int RR = (TFT_WIDTH_PX / 2) - 2;   // 2 px of margin so nothing clips

// --- Palette --------------------------------------------------------------
//
// The same deep-red instrument palette as data/radar.js. #d8f4ff stays
// reserved exclusively for "satellite visible right now" and must never be
// used for chrome.

inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return lgfx::color565(r, g, b); }

const uint16_t C_BG        = rgb(0x06, 0x03, 0x05);
const uint16_t C_GRID_DIM  = rgb(0x3a, 0x12, 0x10);
const uint16_t C_GRID_MID  = rgb(0x5a, 0x1c, 0x17);
const uint16_t C_FLOOR     = rgb(0x6b, 0x24, 0x1d);
const uint16_t C_TEXT      = rgb(0xc9, 0x50, 0x3c);
const uint16_t C_VISIBLE   = rgb(0xd8, 0xf4, 0xff);
const uint16_t C_SWEEP     = rgb(0x1a, 0x0a, 0x09);
const uint16_t C_RADIANT   = rgb(0xe0, 0x29, 0x3f);
const uint16_t C_RADIANT_P = rgb(0xff, 0x6b, 0x57);
const uint16_t C_NEO       = rgb(0xc9, 0x50, 0x3c);
const uint16_t C_NEO_CLOSE = rgb(0xff, 0x6b, 0x57);
const uint16_t C_NEO_MOON  = rgb(0x8a, 0x2f, 0x26);

// --- Backlight ------------------------------------------------------------
//
// A ladder rather than a linear step: perceived brightness is roughly
// logarithmic, so a fixed increment is invisible at the top and slams to black
// at the bottom. The lowest rung is genuinely dim - this thing sits in a dark
// room and the whole red palette exists to protect dark adaptation.
const uint8_t BRIGHTNESS_LADDER[] = {8, 20, 45, 90, 160, 255};
constexpr int LADDER_LEN = sizeof(BRIGHTNESS_LADDER) / sizeof(BRIGHTNESS_LADDER[0]);
int g_rung = 2;   // 45/255

// Idle dim. Gestures are the only input, so an instrument left alone all night
// should not sit at full brightness.
constexpr uint32_t IDLE_DIM_AFTER_MS = 10UL * 60UL * 1000UL;
constexpr uint8_t  IDLE_LEVEL = 6;
uint32_t g_lastWakeMs = 0;
bool     g_dimmed     = false;

constexpr uint32_t FRAME_INTERVAL_MS = 100;   // 10 Hz
uint32_t g_lastFrameMs = 0;
double   g_sweepDeg    = 0.0;

// --- Helpers --------------------------------------------------------------

void polar(double r, double thetaDeg, int& x, int& y) {
    const double a = (thetaDeg - 90.0) * M_PI / 180.0;
    x = CX + static_cast<int>(lround(r * RR * cos(a)));
    y = CY + static_cast<int>(lround(r * RR * sin(a)));
}

uint16_t ringColour(const std::string& kind) {
    if (kind == "floor")   return C_FLOOR;
    if (kind == "horizon") return C_GRID_MID;
    if (kind == "moon")    return C_NEO_MOON;
    return C_GRID_DIM;
}

bool isNeo(const Snapshot& s) { return s.mode == ScopeMode::Neo; }

// Blip radius from apparent magnitude - brighter draws bigger. Same clamp as
// the web renderer, scaled 1.5x for the larger panel.
int blipRadius(double mag) {
    if (!std::isfinite(mag)) return 3;
    double px = 4.8 - mag * 0.68;
    if (px < 2.0) px = 2.0;
    if (px > 7.0) px = 7.0;
    return static_cast<int>(lround(px));
}

int neoRadius(double diameterM) {
    if (!std::isfinite(diameterM) || diameterM <= 0.0) return 3;
    double px = 1.8 + log10(diameterM) * 2.0;
    if (px < 3.0) px = 3.0;
    if (px > 9.0) px = 9.0;
    return static_cast<int>(lround(px));
}

void drawChrome(const Snapshot& s) {
    canvas.fillScreen(C_BG);

    for (const Ring& rg : s.rings) {
        canvas.drawCircle(CX, CY, static_cast<int>(lround(rg.r * RR)), ringColour(rg.kind));
    }

    // Radial ticks every 30 degrees.
    for (int th = 0; th < 360; th += 30) {
        int x1, y1, x2, y2;
        polar(0.955, th, x1, y1);
        polar(1.0, th, x2, y2);
        canvas.drawLine(x1, y1, x2, y2, C_GRID_DIM);
    }

    canvas.setFont(&fonts::Font2);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(C_TEXT, C_BG);

    if (isNeo(s)) {
        // The angular axis is time here, not azimuth.
        const int days = 30;
        char buf[8];
        const int marks[4] = {0, 90, 180, 270};
        for (int i = 0; i < 4; ++i) {
            int x, y;
            polar(0.90, marks[i], x, y);
            if (i == 0) {
                canvas.drawString("NOW", x, y);
            } else {
                std::snprintf(buf, sizeof(buf), "+%dD", (days * i) / 4);
                canvas.drawString(buf, x, y);
            }
        }
    } else {
        const char* names[4] = {"N", "E", "S", "W"};
        const int   angs[4]  = {0, 90, 180, 270};
        for (int i = 0; i < 4; ++i) {
            int x, y;
            polar(0.90, angs[i], x, y);
            canvas.drawString(names[i], x, y);
        }
    }

    // Ring labels. Only NEO mode sends them, because only there does the radius
    // mean something other than an elevation. The innermost goes on the
    // opposite diagonal and slightly outside its own ring: on a 10 LD dial the
    // 1 LD ring is a tenth of the radius, so its label would otherwise land on
    // top of the Earth marker.
    canvas.setFont(&fonts::Font0);
    for (const Ring& rg : s.rings) {
        if (rg.label.empty()) continue;
        const bool moon = (rg.kind == "moon");
        const double rl = moon ? rg.r + 0.07 : std::fmin(rg.r, 0.93);
        int x, y;
        polar(rl, moon ? 135.0 : 225.0, x, y);
        canvas.setTextColor(moon ? C_NEO_CLOSE : C_TEXT, C_BG);
        canvas.drawString(rg.label.c_str(), x, y);
    }
}

void drawSweep() {
    // LovyanGFX measures arc angles from 3 o'clock, clockwise; the dial's
    // theta is measured from north. Hence the -90.
    const float a1 = static_cast<float>(g_sweepDeg - 90.0);
    canvas.fillArc(CX, CY, 0, RR, a1 - 32.0f, a1, C_SWEEP);
}

void drawRadiants(const Snapshot& s) {
    for (const Radiant& rad : s.radiants) {
        int x, y;
        polar(rad.r, rad.theta, x, y);
        int radius = 9 + static_cast<int>(rad.zhr * 0.12);
        if (radius > 30) radius = 30;

        // A radiant is a region of sky to watch, not an object to point at, so
        // it is drawn as a soft patch rather than a hard dot. Concentric rings
        // approximate the web view's radial gradient, which the panel has no
        // alpha channel for.
        const uint16_t c = rad.atPeak ? C_RADIANT_P : C_RADIANT;
        for (int k = radius; k > 0; k -= 3) {
            canvas.drawCircle(x, y, k, c);
        }
    }
}

void drawBlips(const Snapshot& s) {
    for (const Blip& b : s.blips) {
        // Trail first, so the object always reads on top of its own history.
        for (const auto& t : b.trail) {
            int tx, ty;
            polar(t.first, t.second, tx, ty);
            canvas.fillCircle(tx, ty, 1, C_GRID_MID);
        }

        int x, y;
        polar(b.r, b.theta, x, y);
        const uint16_t c = b.visible ? C_VISIBLE : C_TEXT;
        canvas.fillCircle(x, y, blipRadius(b.magnitude), c);

        // Train members fly in formation; a ring around them says "this is one
        // of a group" without needing a connector the panel has no room for.
        if (b.kind == "train") {
            canvas.drawCircle(x, y, blipRadius(b.magnitude) + 3, C_FLOOR);
        }
    }
}

void drawNeos(const Snapshot& s) {
    // Earth at the centre - every radius on this dial is measured from it.
    canvas.fillCircle(CX, CY, 5, C_GRID_MID);

    // The "now" hand. Fixed, not sweeping: a sweep would imply the dial is
    // scanning something live, when it is a 30-day forecast.
    int nx, ny;
    polar(1.0, 0.0, nx, ny);
    canvas.drawLine(CX, CY, nx, ny, C_FLOOR);

    for (const NeoApproachBlip& n : s.neos) {
        int x, y;
        polar(n.r, n.theta, x, y);
        const bool close = n.distLd < 1.0;
        const int  rad   = neoRadius(n.estimatedDiameterM);

        if (close) {
            // Inside the Moon's distance is the one case worth drawing
            // attention to.
            canvas.drawCircle(x, y, rad + 4, C_NEO_CLOSE);
            canvas.drawCircle(x, y, rad + 7, C_NEO_CLOSE);
        }
        canvas.fillCircle(x, y, rad, close ? C_NEO_CLOSE : C_NEO);
    }
}

void statusLine(const Snapshot& s, char* out, size_t n) {
    switch (s.status) {
        case ScopeStatus::NoTime:
            std::snprintf(out, n, "NO TIME");
            return;
        case ScopeStatus::NoLocation:
            // NEO mode is Earth-centred and works without a location, so only
            // SKY mode nags about setup.
            if (!isNeo(s)) { std::snprintf(out, n, "SET LOCATION"); return; }
            break;
        default: break;
    }

    if (isNeo(s)) {
        if (s.neos.empty()) { std::snprintf(out, n, "NO CLOSE PASSES"); return; }
        const NeoApproachBlip& a = s.neos[0];   // sorted by date
        const long days = static_cast<long>(a.approachIn / 86400);
        if (days >= 1) {
            std::snprintf(out, n, "%s %ldD %.1fLD", a.name.c_str(), days, a.distLd);
        } else {
            std::snprintf(out, n, "%s %ldH %.1fLD", a.name.c_str(),
                          static_cast<long>(a.approachIn / 3600), a.distLd);
        }
        return;
    }

    // A pending visible pass outranks the current-position readout.
    if (!s.events.empty()) {
        const Event& e = s.events[0];
        const long mins = e.startsIn / 60 < 0 ? 0 : static_cast<long>(e.startsIn / 60);
        if (mins >= 60) {
            std::snprintf(out, n, "%s %ldH %.0f", e.name.c_str(), mins / 60, e.maxEl);
        } else {
            std::snprintf(out, n, "%s %ldM %.0f", e.name.c_str(), mins, e.maxEl);
        }
        return;
    }

    if (s.status == ScopeStatus::Offline) { std::snprintf(out, n, "OFFLINE"); return; }
    if (s.blips.empty())                  { std::snprintf(out, n, "NOTHING UP"); return; }

    const Blip& b = s.blips[0];
    std::snprintf(out, n, "%s %.0fDEG", b.name.c_str(), b.elevationDeg);
}

void drawStatus(const Snapshot& s) {
    char line[48];
    statusLine(s, line, sizeof(line));

    canvas.setFont(&fonts::Font2);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(C_TEXT, C_BG);
    canvas.drawString(line, CX, CY + static_cast<int>(RR * 0.62));

    if (isNeo(s)) {
        canvas.drawString("NEO", CX, CY - static_cast<int>(RR * 0.70));
    } else if (s.tleAgeHours >= 0.0) {
        char age[16];
        std::snprintf(age, sizeof(age), "TLE %.0fH", s.tleAgeHours);
        canvas.drawString(age, CX, CY - static_cast<int>(RR * 0.70));
    }
}

}  // namespace

bool begin() {
    if (!lcd.init()) {
        Serial.println("[display] GC9B72 init failed - running headless");
        g_present = false;
        return false;
    }
    lcd.setRotation(0);
    lcd.fillScreen(C_BG);

    // 360x360 at 16bpp is 253 KB, which will not fit in internal RAM alongside
    // WiFi and ~200 tracked objects. It goes to PSRAM, where 8 MB is sitting
    // almost unused. Drawing offscreen and pushing once is also what keeps the
    // sweep from tearing.
    canvas.setColorDepth(16);
    canvas.setPsram(true);
    if (!canvas.createSprite(TFT_WIDTH_PX, TFT_HEIGHT_PX)) {
        Serial.println("[display] could not allocate the 360x360 PSRAM canvas");
        g_present = false;
        return false;
    }

    g_present    = true;
    g_lastWakeMs = millis();
    setBrightness(BRIGHTNESS_LADDER[g_rung]);
    Serial.printf("[display] GC9B72 %dx%d ready, canvas in PSRAM (%u bytes free)\n",
                  TFT_WIDTH_PX, TFT_HEIGHT_PX,
                  static_cast<unsigned>(ESP.getFreePsram()));
    return true;
}

bool present() { return g_present; }

void setBrightness(uint8_t level) {
    if (!g_present) return;
    lcd.setBrightness(level);
}

uint8_t brightness() { return BRIGHTNESS_LADDER[g_rung]; }

void nudgeBrightness(int steps) {
    g_rung += steps;
    if (g_rung < 0) g_rung = 0;
    if (g_rung >= LADDER_LEN) g_rung = LADDER_LEN - 1;
    g_dimmed     = false;
    g_lastWakeMs = millis();
    setBrightness(BRIGHTNESS_LADDER[g_rung]);
    Serial.printf("[display] brightness %u/255\n",
                  static_cast<unsigned>(BRIGHTNESS_LADDER[g_rung]));
}

void wake() {
    g_dimmed     = false;
    g_lastWakeMs = millis();
    setBrightness(BRIGHTNESS_LADDER[g_rung]);
}

void loop(const Snapshot& s) {
    if (!g_present) return;

    if (!g_dimmed && (millis() - g_lastWakeMs) > IDLE_DIM_AFTER_MS) {
        g_dimmed = true;
        setBrightness(IDLE_LEVEL);
    }

    if (millis() - g_lastFrameMs < FRAME_INTERVAL_MS) return;
    g_lastFrameMs = millis();

    g_sweepDeg = std::fmod(g_sweepDeg + 4.0, 360.0);

    drawChrome(s);
    if (isNeo(s)) {
        drawNeos(s);
    } else {
        drawSweep();
        drawRadiants(s);
        drawBlips(s);
    }
    drawStatus(s);

    canvas.pushSprite(0, 0);
}

}  // namespace display
