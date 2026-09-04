#include <unity.h>
#include <cmath>
#include "Magnitude.h"
#include "StdMagTable.h"
#include "Vec3.h"

static constexpr double PI = 3.14159265358979323846;

// The defining condition of a standard magnitude: 1000 km range, 90 degree
// phase angle. If the formula's constant is wrong, this is the test that says so.
void test_at_reference_conditions_returns_std_mag(void) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -1.8,
        apparentMagnitude(-1.8, 1000.0, PI / 2.0));
}

void test_doubling_range_dims_by_inverse_square(void) {
    const double near = apparentMagnitude(-1.8, 1000.0, PI / 2.0);
    const double far  = apparentMagnitude(-1.8, 2000.0, PI / 2.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 5.0 * std::log10(2.0), far - near);
}

void test_full_phase_is_brighter_than_quarter_phase(void) {
    const double full    = apparentMagnitude(-1.8, 1000.0, 0.0);
    const double quarter = apparentMagnitude(-1.8, 1000.0, PI / 2.0);
    TEST_ASSERT_TRUE(full < quarter);           // lower magnitude is brighter
    TEST_ASSERT_DOUBLE_WITHIN(0.01, -1.243, full - quarter);
}

void test_brightness_decreases_monotonically_with_phase_angle(void) {
    double prev = -99.0;
    for (double phi = 0.0; phi < PI * 0.98; phi += 0.05) {
        const double m = apparentMagnitude(-1.8, 1000.0, phi);
        TEST_ASSERT_TRUE(m > prev);
        prev = m;
    }
}

void test_back_lit_satellite_is_finite_and_very_dim(void) {
    // Phase angle pi means the observer sees the unlit face. The phase factor
    // collapses to zero, so the result must be clamped rather than infinite.
    const double m = apparentMagnitude(-1.8, 1000.0, PI);
    TEST_ASSERT_TRUE(std::isfinite(m));
    TEST_ASSERT_TRUE(m > 20.0);
}

void test_zero_range_is_finite(void) {
    TEST_ASSERT_TRUE(std::isfinite(apparentMagnitude(-1.8, 0.0, PI / 2.0)));
}

void test_phase_angle_pi_when_the_satellite_is_backlit(void) {
    // Sun far along +x; observer at 6378, satellite at 7000 - so the satellite
    // sits BETWEEN the observer and the Sun and the observer sees its unlit
    // face. That is phase angle pi, not zero.
    Vec3 sun{1.496e8, 0.0, 0.0};
    Vec3 sat{7000.0, 0.0, 0.0};
    Vec3 site{6378.0, 0.0, 0.0};
    TEST_ASSERT_DOUBLE_WITHIN(0.01, PI, phaseAngleRad(sat, sun, site));
}

void test_phase_angle_zero_when_the_satellite_is_fully_lit(void) {
    // Sun behind the OBSERVER: sun, then observer, then satellite on the
    // anti-sun side. The observer sees the fully lit face - phase angle zero.
    //
    // This test and the backlit one above are a matched pair, and together they
    // are the only thing that pins the direction convention. The 90-degree test
    // returns 90 under EITHER convention, so it cannot discriminate: dropping
    // either of these two would let an inverted phaseAngleRad through, which
    // would make every satellite look brightest exactly when it is unlit.
    Vec3 sun{1.496e8, 0.0, 0.0};
    Vec3 sat{-7000.0, 0.0, 0.0};
    Vec3 site{6378.0, 0.0, 0.0};
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.0, phaseAngleRad(sat, sun, site));
}

void test_phase_angle_ninety_degrees(void) {
    // Observer displaced perpendicular to the Sun direction, at the same
    // distance scale as the satellite's offset from it.
    Vec3 sun{1.496e8, 0.0, 0.0};
    Vec3 sat{0.0, 0.0, 0.0};
    Vec3 site{0.0, 1000.0, 0.0};
    TEST_ASSERT_DOUBLE_WITHIN(0.01, PI / 2.0, phaseAngleRad(sat, sun, site));
}

void test_phase_angle_always_in_range(void) {
    Vec3 sun{1.496e8, 0.0, 0.0};
    for (double a = -8000.0; a <= 8000.0; a += 977.0) {
        const double phi = phaseAngleRad(Vec3{a, a * 0.3, -a * 0.7}, sun,
                                         Vec3{6378.0, a * 0.1, 0.0});
        TEST_ASSERT_TRUE(phi >= 0.0);
        TEST_ASSERT_TRUE(phi <= PI + 1e-9);
    }
}

void test_iss_has_a_known_standard_magnitude(void) {
    TEST_ASSERT_DOUBLE_WITHIN(0.01, -1.8, stdMagFor(25544));
}

void test_unknown_object_gets_the_documented_default(void) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, DEFAULT_STD_MAG, stdMagFor(999999));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_at_reference_conditions_returns_std_mag);
    RUN_TEST(test_doubling_range_dims_by_inverse_square);
    RUN_TEST(test_full_phase_is_brighter_than_quarter_phase);
    RUN_TEST(test_brightness_decreases_monotonically_with_phase_angle);
    RUN_TEST(test_back_lit_satellite_is_finite_and_very_dim);
    RUN_TEST(test_zero_range_is_finite);
    RUN_TEST(test_phase_angle_pi_when_the_satellite_is_backlit);
    RUN_TEST(test_phase_angle_zero_when_the_satellite_is_fully_lit);
    RUN_TEST(test_phase_angle_ninety_degrees);
    RUN_TEST(test_phase_angle_always_in_range);
    RUN_TEST(test_iss_has_a_known_standard_magnitude);
    RUN_TEST(test_unknown_object_gets_the_documented_default);
    return UNITY_END();
}
