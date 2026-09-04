#include <unity.h>
#include <cmath>
#include "Solar.h"
#include "TimeUtils.h"
#include "Observer.h"

// Unix timestamps for 12:00 UTC on notable dates in 2000.
static const long long T_J2000    = 946728000LL;  // 2000-01-01, dec approx -23.0
static const long long T_EQUINOX  = 953553600LL;  // 2000-03-20, dec approx 0
static const long long T_SOLSTICE = 961588800LL;  // 2000-06-21, dec approx +23.44

static double jdOf(long long unixSeconds) {
    return timeutils::julianDate(unixSeconds);
}

void test_sun_distance_is_about_one_au(void) {
    // Earth-Sun distance varies between roughly 147.1 and 152.1 million km.
    const double r = sunEci(jdOf(T_J2000)).norm();
    TEST_ASSERT_TRUE(r > 1.44e8);
    TEST_ASSERT_TRUE(r < 1.53e8);
}

void test_sun_is_nearest_in_early_january(void) {
    // Perihelion is in the first week of January, aphelion in early July.
    const double rJan = sunEci(jdOf(T_J2000)).norm();
    const double rJul = sunEci(jdOf(T_SOLSTICE + 15LL * 86400LL)).norm();
    TEST_ASSERT_TRUE(rJan < rJul);
}

void test_declination_at_new_year(void) {
    TEST_ASSERT_DOUBLE_WITHIN(0.3, -23.0, sunDeclinationDeg(jdOf(T_J2000)));
}

void test_declination_near_zero_at_march_equinox(void) {
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 0.0, sunDeclinationDeg(jdOf(T_EQUINOX)));
}

void test_declination_near_maximum_at_june_solstice(void) {
    TEST_ASSERT_DOUBLE_WITHIN(0.3, 23.44, sunDeclinationDeg(jdOf(T_SOLSTICE)));
}

void test_declination_never_exceeds_obliquity(void) {
    for (long long t = T_J2000; t < T_J2000 + 400LL * 86400LL; t += 86400LL * 7LL) {
        const double d = sunDeclinationDeg(jdOf(t));
        TEST_ASSERT_TRUE(std::fabs(d) <= 23.5);
    }
}

void test_sun_is_up_at_local_noon_and_down_at_local_midnight(void) {
    // Greenwich: local noon is 12:00 UTC, local midnight is 00:00 UTC.
    Observer greenwich{51.48, 0.0, 0.0};
    const double noon     = sunAltitudeDeg(greenwich, T_EQUINOX);
    const double midnight = sunAltitudeDeg(greenwich, T_EQUINOX - 43200LL);
    TEST_ASSERT_TRUE(noon > 20.0);
    TEST_ASSERT_TRUE(midnight < -20.0);
}

void test_sun_altitude_stays_in_range(void) {
    Observer obs{37.98, 23.73, 0.1};
    for (long long t = T_J2000; t < T_J2000 + 86400LL * 3LL; t += 997LL) {
        const double a = sunAltitudeDeg(obs, t);
        TEST_ASSERT_TRUE(a >= -90.5);
        TEST_ASSERT_TRUE(a <= 90.5);
    }
}

void test_polar_night_at_the_north_pole_in_january(void) {
    Observer pole{89.9, 0.0, 0.0};
    // The Sun cannot rise at the pole in January regardless of the hour.
    for (long long t = T_J2000; t < T_J2000 + 86400LL; t += 3600LL) {
        TEST_ASSERT_TRUE(sunAltitudeDeg(pole, t) < 0.0);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sun_distance_is_about_one_au);
    RUN_TEST(test_sun_is_nearest_in_early_january);
    RUN_TEST(test_declination_at_new_year);
    RUN_TEST(test_declination_near_zero_at_march_equinox);
    RUN_TEST(test_declination_near_maximum_at_june_solstice);
    RUN_TEST(test_declination_never_exceeds_obliquity);
    RUN_TEST(test_sun_is_up_at_local_noon_and_down_at_local_midnight);
    RUN_TEST(test_sun_altitude_stays_in_range);
    RUN_TEST(test_polar_night_at_the_north_pole_in_january);
    return UNITY_END();
}
