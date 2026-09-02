#include <unity.h>
#include "TimeUtils.h"

// 2000-01-01T12:00:00Z is unix 946728000 and is exactly JD 2451545.0 (J2000.0).
void test_j2000_julian_date(void) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2451545.0, timeutils::julianDate(946728000LL));
}

// The unix epoch itself, 1970-01-01T00:00:00Z, is JD 2440587.5.
void test_unix_epoch_julian_date(void) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2440587.5, timeutils::julianDate(0LL));
}

// One day later must be exactly one Julian day later.
void test_one_day_advances_jd_by_one(void) {
    const double a = timeutils::julianDate(946728000LL);
    const double b = timeutils::julianDate(946728000LL + 86400LL);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, b - a);
}

// GMST at J2000.0 is the standard published value 280.46061837 degrees.
void test_gmst_at_j2000(void) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, 280.46061837, timeutils::gmstDegrees(2451545.0));
}

// GMST must always be normalised into [0, 360).
void test_gmst_is_normalised(void) {
    for (double jd = 2451545.0; jd < 2451545.0 + 5.0; jd += 0.137) {
        const double g = timeutils::gmstDegrees(jd);
        TEST_ASSERT_TRUE(g >= 0.0);
        TEST_ASSERT_TRUE(g < 360.0);
    }
}

// A sidereal day is ~23h56m04s, so after one solar day GMST advances ~360.9856
// degrees, i.e. ~0.9856 degrees modulo a full turn.
void test_gmst_advances_about_one_degree_per_solar_day(void) {
    const double a = timeutils::gmstDegrees(2451545.0);
    const double b = timeutils::gmstDegrees(2451546.0);
    double delta = b - a;
    if (delta < 0.0) delta += 360.0;
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.9856, delta);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_j2000_julian_date);
    RUN_TEST(test_unix_epoch_julian_date);
    RUN_TEST(test_one_day_advances_jd_by_one);
    RUN_TEST(test_gmst_at_j2000);
    RUN_TEST(test_gmst_is_normalised);
    RUN_TEST(test_gmst_advances_about_one_degree_per_solar_day);
    return UNITY_END();
}
