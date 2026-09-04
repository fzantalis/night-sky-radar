#include <unity.h>
#include <cstring>
#include "Visibility.h"

// A geometry that satisfies every condition, used as the baseline that each
// test then breaks in exactly one way.
static const Vec3 SUN {1.496e8, 0.0, 0.0};
static const Vec3 SITE{-6378.0, 0.0, 0.0};      // night side, facing away
static const Vec3 SAT {-6778.0, 7000.0, 0.0};   // above the site, outside shadow

static LookAngles goodLook(void) {
    LookAngles la;
    la.azDeg = 142.0;
    la.elDeg = 45.0;
    la.rangeKm = 800.0;
    return la;
}

void test_all_conditions_met_is_visible(void) {
    Verdict v = judge(goodLook(), SAT, SUN, SITE, -14.0, -1.8);
    TEST_ASSERT_TRUE(v.visible);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VisReason::Visible),
                          static_cast<int>(v.reason));
}

void test_below_the_elevation_floor_is_rejected(void) {
    LookAngles la = goodLook();
    la.elDeg = 9.9;   // just under MIN_ELEVATION_DEG
    Verdict v = judge(la, SAT, SUN, SITE, -14.0, -1.8);
    TEST_ASSERT_FALSE(v.visible);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VisReason::BelowHorizon),
                          static_cast<int>(v.reason));
}

void test_exactly_at_the_elevation_floor_is_rejected(void) {
    LookAngles la = goodLook();
    la.elDeg = MIN_ELEVATION_DEG;
    TEST_ASSERT_FALSE(judge(la, SAT, SUN, SITE, -14.0, -1.8).visible);
}

void test_daylight_is_rejected(void) {
    Verdict v = judge(goodLook(), SAT, SUN, SITE, -5.9, -1.8);
    TEST_ASSERT_FALSE(v.visible);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VisReason::Daylight),
                          static_cast<int>(v.reason));
}

void test_civil_twilight_boundary_is_rejected(void) {
    TEST_ASSERT_FALSE(judge(goodLook(), SAT, SUN, SITE,
                            MAX_SUN_ALT_DEG, -1.8).visible);
}

void test_eclipsed_satellite_is_rejected(void) {
    Vec3 shadowed{-6778.0, 0.0, 0.0};   // squarely in the shadow cylinder
    Verdict v = judge(goodLook(), shadowed, SUN, SITE, -14.0, -1.8);
    TEST_ASSERT_FALSE(v.visible);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VisReason::Eclipsed),
                          static_cast<int>(v.reason));
}

void test_daylight_beats_eclipsed_when_both_hold(void) {
    // Every other test isolates exactly one failing condition, so nothing
    // pins the relative order of the darkness and illumination gates: a
    // shadowed satellite in full daylight, with elevation passing, must be
    // reported as Daylight (darkness gate runs first), not Eclipsed. If the
    // two gates were swapped in judge(), this would be the only test to
    // notice.
    Vec3 shadowed{-6778.0, 0.0, 0.0};   // squarely in the shadow cylinder,
                                        // same fixture as test_eclipsed_satellite_is_rejected
    Verdict v = judge(goodLook(), shadowed, SUN, SITE, /*sunAltDeg=*/10.0, -1.8);
    TEST_ASSERT_FALSE(v.visible);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VisReason::Daylight),
                          static_cast<int>(v.reason));
}

void test_too_dim_is_rejected(void) {
    Verdict v = judge(goodLook(), SAT, SUN, SITE, -14.0, 9.0);
    TEST_ASSERT_FALSE(v.visible);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VisReason::TooDim),
                          static_cast<int>(v.reason));
}

void test_magnitude_is_reported_even_when_rejected(void) {
    // The renderer sizes blips by magnitude regardless of visibility, so the
    // number must be filled in on every path that got far enough to compute it.
    Verdict v = judge(goodLook(), SAT, SUN, SITE, -14.0, 9.0);
    TEST_ASSERT_TRUE(v.magnitude > 4.5);
    TEST_ASSERT_TRUE(v.magnitude < 30.0);
}

void test_horizon_check_precedes_everything(void) {
    // An object below the floor in daylight and eclipsed reports BelowHorizon,
    // so the reason is stable and does not depend on evaluation order.
    LookAngles la = goodLook();
    la.elDeg = -30.0;
    Vec3 shadowed{-6778.0, 0.0, 0.0};
    Verdict v = judge(la, shadowed, SUN, SITE, 30.0, -1.8);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VisReason::BelowHorizon),
                          static_cast<int>(v.reason));
}

void test_reason_names_are_stable(void) {
    TEST_ASSERT_EQUAL_STRING("visible",       visReasonName(VisReason::Visible));
    TEST_ASSERT_EQUAL_STRING("below_horizon", visReasonName(VisReason::BelowHorizon));
    TEST_ASSERT_EQUAL_STRING("daylight",      visReasonName(VisReason::Daylight));
    TEST_ASSERT_EQUAL_STRING("eclipsed",      visReasonName(VisReason::Eclipsed));
    TEST_ASSERT_EQUAL_STRING("too_dim",       visReasonName(VisReason::TooDim));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_all_conditions_met_is_visible);
    RUN_TEST(test_below_the_elevation_floor_is_rejected);
    RUN_TEST(test_exactly_at_the_elevation_floor_is_rejected);
    RUN_TEST(test_daylight_is_rejected);
    RUN_TEST(test_civil_twilight_boundary_is_rejected);
    RUN_TEST(test_eclipsed_satellite_is_rejected);
    RUN_TEST(test_daylight_beats_eclipsed_when_both_hold);
    RUN_TEST(test_too_dim_is_rejected);
    RUN_TEST(test_magnitude_is_reported_even_when_rejected);
    RUN_TEST(test_horizon_check_precedes_everything);
    RUN_TEST(test_reason_names_are_stable);
    return UNITY_END();
}
