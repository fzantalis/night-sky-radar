#include <unity.h>
#include "Projection.h"

void test_zenith_maps_to_centre(void) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, skyRadius(90.0));
}

void test_horizon_maps_to_rim(void) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, skyRadius(0.0));
}

void test_visibility_floor_maps_to_expected_radius(void) {
    // Elevation 10 degrees is the visibility floor ring: (90-10)/90 = 0.888...
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.8888889, skyRadius(10.0));
}

void test_midpoint_elevation(void) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.5, skyRadius(45.0));
}

void test_below_horizon_clamps_to_rim(void) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, skyRadius(-20.0));
}

void test_above_zenith_clamps_to_centre(void) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, skyRadius(95.0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_zenith_maps_to_centre);
    RUN_TEST(test_horizon_maps_to_rim);
    RUN_TEST(test_visibility_floor_maps_to_expected_radius);
    RUN_TEST(test_midpoint_elevation);
    RUN_TEST(test_below_horizon_clamps_to_rim);
    RUN_TEST(test_above_zenith_clamps_to_centre);
    return UNITY_END();
}
