#include <unity.h>
#include "Eclipse.h"
#include "Vec3.h"

// Sun placed a long way down the +x axis for all these cases.
static const Vec3 SUN{1.496e8, 0.0, 0.0};

void test_satellite_on_the_sunward_side_is_lit(void) {
    Vec3 sat{6778.0, 0.0, 0.0};   // 400 km up, directly toward the Sun
    TEST_ASSERT_TRUE(isSunlit(sat, SUN));
}

void test_satellite_directly_behind_earth_is_eclipsed(void) {
    Vec3 sat{-6778.0, 0.0, 0.0};  // 400 km up on the anti-sun side
    TEST_ASSERT_FALSE(isSunlit(sat, SUN));
}

void test_satellite_behind_earth_but_outside_the_shadow_cylinder_is_lit(void) {
    // Anti-sun side, but displaced further from the Earth-Sun axis than one
    // Earth radius, so the shadow cylinder misses it.
    Vec3 sat{-6778.0, 7000.0, 0.0};
    TEST_ASSERT_TRUE(isSunlit(sat, SUN));
}

void test_satellite_just_inside_the_shadow_cylinder_is_eclipsed(void) {
    Vec3 sat{-6778.0, 6000.0, 0.0};   // 6000 km off-axis, inside 6378
    TEST_ASSERT_FALSE(isSunlit(sat, SUN));
}

void test_terminator_crossing_is_monotonic(void) {
    // Sweeping the off-axis distance outward must cross from eclipsed to lit
    // exactly once, at the Earth's radius.
    bool sawEclipsed = false, sawLit = false;
    for (double y = 0.0; y < 12000.0; y += 100.0) {
        const bool lit = isSunlit(Vec3{-6778.0, y, 0.0}, SUN);
        if (!lit) {
            sawEclipsed = true;
            TEST_ASSERT_FALSE(sawLit);   // must not flip back
        } else {
            sawLit = true;
        }
    }
    TEST_ASSERT_TRUE(sawEclipsed);
    TEST_ASSERT_TRUE(sawLit);
}

void test_high_altitude_satellite_over_the_pole_stays_lit(void) {
    // A satellite far above the pole is never in the equatorial shadow when the
    // Sun lies in the equatorial plane.
    Vec3 sat{-30000.0, 0.0, 25000.0};
    TEST_ASSERT_TRUE(isSunlit(sat, SUN));
}

void test_zero_sun_vector_defaults_to_lit(void) {
    // Degenerate input must not produce NaN-driven behaviour.
    TEST_ASSERT_TRUE(isSunlit(Vec3{-6778.0, 0.0, 0.0}, Vec3{0.0, 0.0, 0.0}));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_satellite_on_the_sunward_side_is_lit);
    RUN_TEST(test_satellite_directly_behind_earth_is_eclipsed);
    RUN_TEST(test_satellite_behind_earth_but_outside_the_shadow_cylinder_is_lit);
    RUN_TEST(test_satellite_just_inside_the_shadow_cylinder_is_eclipsed);
    RUN_TEST(test_terminator_crossing_is_monotonic);
    RUN_TEST(test_high_altitude_satellite_over_the_pole_stays_lit);
    RUN_TEST(test_zero_sun_vector_defaults_to_lit);
    return UNITY_END();
}
