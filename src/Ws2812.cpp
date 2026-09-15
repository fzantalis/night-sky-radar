#include "Ws2812.h"

#include <Arduino.h>

namespace ws2812 {

namespace {

// Each pixel is 24 RMT symbols, plus one trailing reset symbol for the whole
// frame. Two pixels is 49 symbols, which would fit a single 64-symbol block,
// but the channel is allocated as RMT_MEM_128 so the pixel count can be changed
// without silently overrunning: three pixels is already 73.
//
// MAX_PIXELS is what the 128-symbol allocation can actually hold -
// 5 * 24 + 1 = 121 - rather than a round number. Overrunning it does not fail
// loudly; it corrupts the tail of the frame, which looks like one stubbornly
// wrong pixel at the far end of the strip.
constexpr int MAX_PIXELS = 5;
constexpr int BITS_PER_PIXEL = 24;
constexpr int MAX_SYMBOLS = MAX_PIXELS * BITS_PER_PIXEL + 1;

rmt_obj_t* g_rmt  = nullptr;
int        g_count = 0;

rmt_data_t g_buf[MAX_SYMBOLS];

}  // namespace

bool begin(int pin, int count) {
    if (count <= 0 || count > MAX_PIXELS) {
        Serial.printf("[ws2812] refusing %d pixels (max %d)\n", count, MAX_PIXELS);
        return false;
    }

    g_rmt = rmtInit(pin, RMT_TX_MODE, RMT_MEM_128);
    if (g_rmt == nullptr) {
        Serial.printf("[ws2812] rmtInit failed on GPIO%d\n", pin);
        return false;
    }
    rmtSetTick(g_rmt, 100);   // 100 ns per tick
    g_count = count;
    Serial.printf("[ws2812] %d pixels on GPIO%d\n", count, pin);

    // Start dark rather than inheriting whatever the pixels powered up with.
    LedColor off[MAX_PIXELS];
    show(off, count);
    return true;
}

bool ready() { return g_rmt != nullptr; }

void show(const LedColor* px, int count) {
    if (g_rmt == nullptr || px == nullptr) return;
    if (count > g_count) count = g_count;
    if (count <= 0) return;

    int i = 0;
    for (int p = 0; p < count; ++p) {
        const uint8_t colour[3] = {px[p].g, px[p].r, px[p].b};   // GRB on the wire
        for (int c = 0; c < 3; ++c) {
            for (int bit = 0; bit < 8; ++bit) {
                const bool high = colour[c] & (1 << (7 - bit));
                g_buf[i].level0    = 1;
                g_buf[i].duration0 = high ? 8 : 4;
                g_buf[i].level1    = 0;
                g_buf[i].duration1 = high ? 4 : 8;
                ++i;
            }
        }
    }

    // Trailing reset: the WS2812 latches after the line is held low for more
    // than 50 us. 500 ticks at 100 ns is exactly that, and leaving it to the
    // gap between calls would make latching depend on caller timing.
    g_buf[i].level0    = 0;
    g_buf[i].duration0 = 500;
    g_buf[i].level1    = 0;
    g_buf[i].duration1 = 500;
    ++i;

    // Blocking, not rmtWrite(), and this is load-bearing rather than cautious.
    //
    // rmtWrite() calls rmt_tx_stop() before queuing the new frame. If the
    // previous frame is still transmitting, that stop prevents its TX-done
    // interrupt from ever firing, so the driver's tx semaphore is never
    // returned - and rmt_write_items() then waits on that semaphore with
    // portMAX_DELAY. The result is a silent, permanent deadlock: no crash, no
    // watchdog, just a board that stops mid-boot with no output.
    //
    // It surfaced the first time two frames were issued microseconds apart
    // (the boot self-test). The 20 Hz update path never collided, so the bug
    // sat latent. rmtWriteBlocking() returns only once the frame has actually
    // finished, which guarantees the channel is idle for the next call and
    // removes the hazard entirely. A two-pixel frame is ~200 us, so the cost
    // is irrelevant at any rate this is called.
    rmtWriteBlocking(g_rmt, g_buf, i);
}

}  // namespace ws2812
