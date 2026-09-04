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
    RUN_TEST(test_a_nearer_pass_pulses_faster);
    return UNITY_END();
}
