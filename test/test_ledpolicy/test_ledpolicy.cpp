#include <unity.h>
#include "LedPolicy.h"
#include "Snapshot.h"

static Snapshot okSnapshot(void) {
    Snapshot s;
    s.status = ScopeStatus::Ok;
    s.t = 1788400000LL;
    return s;
}

static Event visiblePassIn(int64_t seconds) {
    Event e;
    e.name = "ISS (ZARYA)";
    e.startsIn = seconds;
    e.maxEl = 67.0;
    e.visible = true;
    return e;
}

static bool isOff(const LedColor& c) {
    return c.r == 0 && c.g == 0 && c.b == 0;
}

static uint8_t maxChannel(const LedColor& c) {
    uint8_t m = c.r;
    if (c.g > m) m = c.g;
    if (c.b > m) m = c.b;
    return m;
}

void test_quiet_sky_is_dark(void) {
    // Nothing visible, nothing imminent: the LED must be fully off, not dimly
    // lit. This sits in a dark room.
    TEST_ASSERT_TRUE(isOff(ledFor(okSnapshot(), 0)));
    TEST_ASSERT_TRUE(isOff(ledFor(okSnapshot(), 1234)));
}

void test_no_time_is_signalled(void) {
    Snapshot s = okSnapshot();
    s.status = ScopeStatus::NoTime;
    const LedColor c = ledFor(s, 0);
    TEST_ASSERT_FALSE(isOff(c));
    TEST_ASSERT_TRUE(c.r > c.b);          // amber-ish, warm
}

void test_no_location_is_signalled(void) {
    Snapshot s = okSnapshot();
    s.status = ScopeStatus::NoLocation;
    TEST_ASSERT_FALSE(isOff(ledFor(s, 0)));
}

void test_imminent_visible_pass_pulses_red(void) {
    Snapshot s = okSnapshot();
    s.events.push_back(visiblePassIn(300));
    const LedColor c = ledFor(s, 0);
    TEST_ASSERT_FALSE(isOff(c));
    TEST_ASSERT_TRUE(c.r > c.g);
    TEST_ASSERT_TRUE(c.r > c.b);
}

void test_pass_beyond_the_alert_window_is_dark(void) {
    Snapshot s = okSnapshot();
    s.events.push_back(visiblePassIn(ALERT_WINDOW_SEC + 60));
    TEST_ASSERT_TRUE(isOff(ledFor(s, 0)));
}

void test_non_visible_pass_never_alerts(void) {
    // Only naked-eye passes are worth walking outside for.
    Snapshot s = okSnapshot();
    Event e = visiblePassIn(120);
    e.visible = false;
    s.events.push_back(e);
    TEST_ASSERT_TRUE(isOff(ledFor(s, 0)));
}

void test_object_visible_right_now_uses_the_reserved_cool_colour(void) {
    // #d8f4ff is reserved project-wide for "visible right now" and must not be
    // confused with the warm alert colour.
    Snapshot s = okSnapshot();
    Blip b;
    b.id = 25544;
    b.name = "ISS (ZARYA)";
    b.visible = true;
    s.blips.push_back(b);
    const LedColor c = ledFor(s, 0);
    TEST_ASSERT_FALSE(isOff(c));
    TEST_ASSERT_TRUE(c.b >= c.r);         // cool, not warm
}

void test_visible_now_outranks_an_imminent_pass(void) {
    Snapshot s = okSnapshot();
    s.events.push_back(visiblePassIn(60));
    Blip b;
    b.visible = true;
    s.blips.push_back(b);
    const LedColor c = ledFor(s, 0);
    TEST_ASSERT_TRUE(c.b >= c.r);         // the cool "it is up NOW" colour wins
}

void test_brightness_breathes_over_phase(void) {
    Snapshot s = okSnapshot();
    s.events.push_back(visiblePassIn(300));
    bool differs = false;
    const LedColor first = ledFor(s, 0);
    for (uint32_t ph = 50; ph < 4000; ph += 50) {
        if (maxChannel(ledFor(s, ph)) != maxChannel(first)) { differs = true; break; }
    }
    TEST_ASSERT_TRUE(differs);
}

void test_brightness_never_exceeds_the_cap(void) {
    // This is a night instrument. A full-brightness WS2812 is blinding in a
    // dark room and destroys dark adaptation.
    Snapshot s = okSnapshot();
    s.events.push_back(visiblePassIn(30));
    Blip b; b.visible = true; s.blips.push_back(b);
    for (uint32_t ph = 0; ph < 8000; ph += 25) {
        const LedColor c = ledFor(s, ph);
        TEST_ASSERT_TRUE(c.r <= LED_MAX_BRIGHTNESS);
        TEST_ASSERT_TRUE(c.g <= LED_MAX_BRIGHTNESS);
        TEST_ASSERT_TRUE(c.b <= LED_MAX_BRIGHTNESS);
    }
}

void test_colour_constants_respect_brightness_cap(void) {
    // The clamp in scale() is a guard against future colour-formula changes.
    // This test pins the invariant it exists to protect: no colour constant
    // used by ledFor can exceed LED_MAX_BRIGHTNESS. If this test fails after
    // a colour constant edit, the clamp may have been bypassed.
    TEST_ASSERT_TRUE(ColorChannels::SETUP_RED <= LED_MAX_BRIGHTNESS);
    TEST_ASSERT_TRUE(ColorChannels::SETUP_GREEN <= LED_MAX_BRIGHTNESS);
    TEST_ASSERT_TRUE(ColorChannels::SETUP_BLUE <= LED_MAX_BRIGHTNESS);

    TEST_ASSERT_TRUE(ColorChannels::VISIBLE_RED <= LED_MAX_BRIGHTNESS);
    TEST_ASSERT_TRUE(ColorChannels::VISIBLE_GREEN <= LED_MAX_BRIGHTNESS);
    TEST_ASSERT_TRUE(ColorChannels::VISIBLE_BLUE <= LED_MAX_BRIGHTNESS);

    TEST_ASSERT_TRUE(ColorChannels::ALERT_RED <= LED_MAX_BRIGHTNESS);
    TEST_ASSERT_TRUE(ColorChannels::ALERT_GREEN <= LED_MAX_BRIGHTNESS);
    TEST_ASSERT_TRUE(ColorChannels::ALERT_BLUE <= LED_MAX_BRIGHTNESS);
}

void test_a_nearer_pass_pulses_faster(void) {
    // Urgency should be legible without reading the dial. Count how many times
    // brightness reverses direction over a fixed window; a shorter period
    // reverses more often.
    Snapshot near_ = okSnapshot();
    near_.events.push_back(visiblePassIn(30));
    Snapshot far_ = okSnapshot();
    far_.events.push_back(visiblePassIn(540));

    int nearTurns = 0, farTurns = 0;
    for (uint32_t ph = 40; ph < 6000; ph += 20) {
        const int nPrev = maxChannel(ledFor(near_, ph - 40));
        const int nMid  = maxChannel(ledFor(near_, ph - 20));
        const int nNow  = maxChannel(ledFor(near_, ph));
        if ((nMid - nPrev) * (nNow - nMid) < 0) nearTurns++;

        const int fPrev = maxChannel(ledFor(far_, ph - 40));
        const int fMid  = maxChannel(ledFor(far_, ph - 20));
        const int fNow  = maxChannel(ledFor(far_, ph));
        if ((fMid - fPrev) * (fNow - fMid) < 0) farTurns++;
    }
    TEST_ASSERT_TRUE(nearTurns > farTurns);
}


// ------------------------------------------------------- M6: countdown bar

static int litCount(const LedStrip& st) {
    int n = 0;
    for (int i = 0; i < LED_COUNT; ++i) {
        if (!isOff(st.px[i])) n++;
    }
    return n;
}

// The stage boundary, pinned from both sides. A bar that lit a constant number
// of pixels, or got the comparison backwards, fails here.
void test_countdown_bar_stages(void) {
    TEST_ASSERT_EQUAL_INT(1, countdownPixels(ALERT_WINDOW_SEC));       // window edge
    TEST_ASSERT_EQUAL_INT(1, countdownPixels(ALERT_STAGE_2_SEC + 1));
    TEST_ASSERT_EQUAL_INT(2, countdownPixels(ALERT_STAGE_2_SEC));      // boundary is inclusive
    TEST_ASSERT_EQUAL_INT(2, countdownPixels(0));
}

// Stated against LED_COUNT rather than a literal 2, so that changing the strip
// length fails loudly here instead of silently capping the bar - which is
// exactly what happened going from three pixels to two.
void test_final_stage_lights_the_whole_bar(void) {
    TEST_ASSERT_EQUAL_INT(LED_COUNT, countdownPixels(0));
    TEST_ASSERT_EQUAL_INT(LED_COUNT, countdownPixels(ALERT_STAGE_2_SEC));
}

// Nothing outside the window lights the bar at all, including a pass that has
// already begun - a negative startsIn must not wrap into "urgent".
void test_countdown_bar_is_dark_outside_the_window(void) {
    TEST_ASSERT_EQUAL_INT(0, countdownPixels(601));
    TEST_ASSERT_EQUAL_INT(0, countdownPixels(100000));
    TEST_ASSERT_EQUAL_INT(0, countdownPixels(-1));
    TEST_ASSERT_EQUAL_INT(0, countdownPixels(-100000));
}

// The bar must never shrink as a pass gets closer. Sweeping the whole window
// catches a non-monotonic mapping that spot checks at the boundaries could
// step straight over.
void test_countdown_bar_never_shrinks_as_the_pass_approaches(void) {
    int previous = 0;
    for (int64_t t = ALERT_WINDOW_SEC; t >= 0; --t) {
        const int n = countdownPixels(t);
        TEST_ASSERT_TRUE(n >= previous);
        TEST_ASSERT_TRUE(n >= 0 && n <= LED_COUNT);
        previous = n;
    }
    TEST_ASSERT_EQUAL_INT(LED_COUNT, previous);
}

void test_strip_is_dark_on_a_quiet_sky(void) {
    const LedStrip st = ledStripFor(okSnapshot(), 0);
    TEST_ASSERT_EQUAL_INT(0, litCount(st));
}

void test_strip_counts_down_towards_a_pass(void) {
    Snapshot s = okSnapshot();

    s.events.clear();
    s.events.push_back(visiblePassIn(540));
    TEST_ASSERT_EQUAL_INT(1, litCount(ledStripFor(s, 0)));

    s.events.clear();
    s.events.push_back(visiblePassIn(200));
    TEST_ASSERT_EQUAL_INT(2, litCount(ledStripFor(s, 0)));

    // Closer still cannot light more than the strip has; the remaining urgency
    // is carried by the pulse rate, which test_a_nearer_pass_pulses_faster pins.
    s.events.clear();
    s.events.push_back(visiblePassIn(30));
    TEST_ASSERT_EQUAL_INT(LED_COUNT, litCount(ledStripFor(s, 0)));
}

// The bar fills from pixel 0 - the end nearest the data input - so it always
// grows from the same physical end.
void test_strip_fills_from_the_first_pixel(void) {
    Snapshot s = okSnapshot();
    s.events.push_back(visiblePassIn(540));
    const LedStrip st = ledStripFor(s, 0);
    TEST_ASSERT_FALSE(isOff(st.px[0]));
    for (int i = 1; i < LED_COUNT; ++i) {
        TEST_ASSERT_TRUE(isOff(st.px[i]));
    }
}

// Setup and "visible right now" are whole-instrument states, so the bar is
// uniform rather than partially filled - a partial bar there would read as a
// countdown to nothing.
void test_whole_bar_lights_for_setup_and_visible_states(void) {
    Snapshot noTime;
    noTime.status = ScopeStatus::NoTime;
    TEST_ASSERT_EQUAL_INT(LED_COUNT, litCount(ledStripFor(noTime, 500)));

    Snapshot vis = okSnapshot();
    Blip b;
    b.name = "ISS (ZARYA)";
    b.visible = true;
    vis.blips.push_back(b);
    TEST_ASSERT_EQUAL_INT(LED_COUNT, litCount(ledStripFor(vis, 500)));
}

// Whatever the bar lights, it must agree with the single-pixel policy about
// the colour - the two entry points share one state classifier precisely so
// they cannot drift.
void test_strip_colour_matches_the_single_pixel_policy(void) {
    Snapshot s = okSnapshot();
    s.events.push_back(visiblePassIn(120));
    const uint32_t phase = 777;
    const LedColor one = ledFor(s, phase);
    const LedStrip st  = ledStripFor(s, phase);
    TEST_ASSERT_EQUAL_UINT8(one.r, st.px[0].r);
    TEST_ASSERT_EQUAL_UINT8(one.g, st.px[0].g);
    TEST_ASSERT_EQUAL_UINT8(one.b, st.px[0].b);
}

// The night-adaptation cap is the whole reason this instrument is dim; three
// pixels must not quietly become three times a bright one.
void test_every_pixel_respects_the_brightness_cap(void) {
    Snapshot s = okSnapshot();
    s.events.push_back(visiblePassIn(10));
    for (uint32_t phase = 0; phase < 5000; phase += 37) {
        const LedStrip st = ledStripFor(s, phase);
        for (int i = 0; i < LED_COUNT; ++i) {
            TEST_ASSERT_TRUE(maxChannel(st.px[i]) <= LED_MAX_BRIGHTNESS);
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_quiet_sky_is_dark);
    RUN_TEST(test_no_time_is_signalled);
    RUN_TEST(test_no_location_is_signalled);
    RUN_TEST(test_imminent_visible_pass_pulses_red);
    RUN_TEST(test_pass_beyond_the_alert_window_is_dark);
    RUN_TEST(test_non_visible_pass_never_alerts);
    RUN_TEST(test_object_visible_right_now_uses_the_reserved_cool_colour);
    RUN_TEST(test_visible_now_outranks_an_imminent_pass);
    RUN_TEST(test_brightness_breathes_over_phase);
    RUN_TEST(test_brightness_never_exceeds_the_cap);
    RUN_TEST(test_colour_constants_respect_brightness_cap);
    RUN_TEST(test_a_nearer_pass_pulses_faster);
    RUN_TEST(test_countdown_bar_stages);
    RUN_TEST(test_final_stage_lights_the_whole_bar);
    RUN_TEST(test_countdown_bar_is_dark_outside_the_window);
    RUN_TEST(test_countdown_bar_never_shrinks_as_the_pass_approaches);
    RUN_TEST(test_strip_is_dark_on_a_quiet_sky);
    RUN_TEST(test_strip_counts_down_towards_a_pass);
    RUN_TEST(test_strip_fills_from_the_first_pixel);
    RUN_TEST(test_whole_bar_lights_for_setup_and_visible_states);
    RUN_TEST(test_strip_colour_matches_the_single_pixel_policy);
    RUN_TEST(test_every_pixel_respects_the_brightness_cap);
    return UNITY_END();
}
