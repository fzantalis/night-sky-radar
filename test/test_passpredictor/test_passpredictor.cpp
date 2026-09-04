#include <unity.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include "PassPredictor.h"
#include "Propagator.h"
#include "Tle.h"
#include "Observer.h"
#include "TimeUtils.h"
#include "Topocentric.h"
#include "Visibility.h"

// ISS element set. The absolute epoch matters only in that predictions are made
// relative to it, so the test asks for passes starting at the epoch itself.
static const char* ISS_NAME = "ISS (ZARYA)";
static const char* ISS_L1 =
    "1 25544U 98067A   24001.50000000  .00016717  00000-0  30074-3 0  9991";
static const char* ISS_L2 =
    "2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537";

static Propagator issProp(void) {
    Tle t;
    TEST_ASSERT_TRUE(parseTle(ISS_NAME, ISS_L1, ISS_L2, t));
    Propagator p;
    TEST_ASSERT_TRUE(p.init(t));
    return p;
}

static int64_t issEpochUnix(void) {
    Propagator p = issProp();
    return static_cast<int64_t>((p.epochJd() - 2440587.5) * 86400.0);
}

// --- the inclination pre-filter -------------------------------------------

void test_iss_can_rise_at_athens(void) {
    TEST_ASSERT_TRUE(couldEverRise(51.6416, 15.72125391, 37.98));
}

void test_iss_can_rise_just_above_its_inclination(void) {
    // An orbit reaches about 11 degrees beyond its inclination at the 10 degree
    // elevation floor, so 60N is still reachable for the ISS.
    TEST_ASSERT_TRUE(couldEverRise(51.6416, 15.72125391, 60.0));
}

void test_iss_cannot_rise_at_the_north_pole(void) {
    TEST_ASSERT_FALSE(couldEverRise(51.6416, 15.72125391, 89.0));
}

void test_polar_orbit_can_rise_anywhere(void) {
    TEST_ASSERT_TRUE(couldEverRise(98.0, 15.2, 89.0));
    TEST_ASSERT_TRUE(couldEverRise(98.0, 15.2, 0.0));
}

void test_equatorial_orbit_cannot_rise_at_high_latitude(void) {
    TEST_ASSERT_FALSE(couldEverRise(0.5, 15.5, 70.0));
}

void test_filter_is_symmetric_about_the_equator(void) {
    TEST_ASSERT_EQUAL_INT(couldEverRise(51.6416, 15.72125391, 60.0) ? 1 : 0,
                          couldEverRise(51.6416, 15.72125391, -60.0) ? 1 : 0);
}

// --- the search ------------------------------------------------------------

void test_iss_has_several_passes_over_athens_in_a_day(void) {
    Observer athens{37.98, 23.73, 0.1};
    std::vector<Pass> p = predictPasses(issProp(), athens, -1.8,
                                        issEpochUnix(), 24, nullptr);
    // A 51.6 degree orbit gives a mid-latitude site roughly 4-8 passes a day
    // above 10 degrees.
    TEST_ASSERT_TRUE(p.size() >= 3);
    TEST_ASSERT_TRUE(p.size() <= 12);
}

void test_passes_are_internally_ordered(void) {
    Observer athens{37.98, 23.73, 0.1};
    std::vector<Pass> p = predictPasses(issProp(), athens, -1.8,
                                        issEpochUnix(), 24, nullptr);
    for (const Pass& x : p) {
        TEST_ASSERT_TRUE(x.riseUnix < x.maxUnix);
        TEST_ASSERT_TRUE(x.maxUnix  < x.setUnix);
    }
}

void test_passes_are_chronological_and_disjoint(void) {
    Observer athens{37.98, 23.73, 0.1};
    std::vector<Pass> p = predictPasses(issProp(), athens, -1.8,
                                        issEpochUnix(), 24, nullptr);
    for (size_t i = 1; i < p.size(); ++i) {
        TEST_ASSERT_TRUE(p[i].riseUnix > p[i - 1].setUnix);
    }
}

void test_every_pass_clears_the_elevation_floor(void) {
    Observer athens{37.98, 23.73, 0.1};
    std::vector<Pass> p = predictPasses(issProp(), athens, -1.8,
                                        issEpochUnix(), 24, nullptr);
    for (const Pass& x : p) {
        TEST_ASSERT_TRUE(x.maxElDeg > 10.0);
        TEST_ASSERT_TRUE(x.maxElDeg <= 90.0);
    }
}

void test_passes_last_a_plausible_number_of_minutes(void) {
    Observer athens{37.98, 23.73, 0.1};
    std::vector<Pass> p = predictPasses(issProp(), athens, -1.8,
                                        issEpochUnix(), 24, nullptr);
    for (const Pass& x : p) {
        const int64_t seconds = x.setUnix - x.riseUnix;
        TEST_ASSERT_TRUE(seconds > 30);      // shortest grazing pass
        TEST_ASSERT_TRUE(seconds < 900);     // longest possible for LEO
    }
}

void test_no_passes_where_the_filter_says_impossible(void) {
    Observer pole{89.0, 0.0, 0.0};
    std::vector<Pass> p = predictPasses(issProp(), pole, -1.8,
                                        issEpochUnix(), 24, nullptr);
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(p.size()));
}

void test_yield_callback_is_invoked(void) {
    static int calls = 0;
    calls = 0;
    Observer athens{37.98, 23.73, 0.1};
    predictPasses(issProp(), athens, -1.8, issEpochUnix(), 24,
                  []() { calls++; });
    TEST_ASSERT_TRUE(calls > 0);
}

void test_zero_hour_horizon_returns_nothing(void) {
    Observer athens{37.98, 23.73, 0.1};
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(
        predictPasses(issProp(), athens, -1.8, issEpochUnix(), 0, nullptr).size()));
}

// --- refineCrossing bisection (regression for the set-side dead-loop bug) --

// Independently recomputes elevation at a given instant from the same TLE,
// the same way PassPredictor's internal elevationAt() does it. Deliberately
// duplicated rather than reusing PassPredictor's anonymous-namespace helper,
// so this test exercises the public look()/gmstDegrees()/julianDate() path
// on its own and cannot silently share a bug with the code under test.
static double independentElevationDeg(Propagator& prop, const Observer& obs, int64_t t) {
    Vec3 pos;
    TEST_ASSERT_TRUE(prop.positionAt(t, pos));
    const double gmst = timeutils::gmstDegrees(timeutils::julianDate(t));
    return look(pos, obs, gmst).elDeg;
}

void test_rise_and_set_land_on_the_elevation_floor(void) {
    Observer athens{37.98, 23.73, 0.1};
    std::vector<Pass> passes = predictPasses(issProp(), athens, -1.8,
                                             issEpochUnix(), 24, nullptr);
    TEST_ASSERT_TRUE(passes.size() > 0);

    Propagator checkProp = issProp();
    for (const Pass& p : passes) {
        const double riseEl = independentElevationDeg(checkProp, athens, p.riseUnix);
        const double setEl  = independentElevationDeg(checkProp, athens, p.setUnix);
        // A properly bisected crossing lands within about a second of the
        // true crossing, so its elevation is essentially exactly the floor.
        // An unrefined 30-second grid sample (the set-side dead-loop bug)
        // is off by a degree or more for a LEO pass.
        TEST_ASSERT_TRUE(std::fabs(riseEl - MIN_ELEVATION_DEG) < 0.2);
        TEST_ASSERT_TRUE(std::fabs(setEl  - MIN_ELEVATION_DEG) < 0.2);
    }
}

// --- Pass::visible wiring ---------------------------------------------------

void test_visible_flag_distinguishes_passes_over_a_week(void) {
    Observer athens{37.98, 23.73, 0.1};
    std::vector<Pass> passes = predictPasses(issProp(), athens, -1.8,
                                             issEpochUnix(), 24 * 7, nullptr);
    TEST_ASSERT_TRUE(passes.size() > 0);

    int visibleCount = 0;
    int notVisibleCount = 0;
    for (const Pass& p : passes) {
        if (p.visible) {
            ++visibleCount;
        } else {
            ++notVisibleCount;
        }
    }
    std::printf("test_visible_flag_distinguishes_passes_over_a_week: "
                "%d passes, %d visible, %d not visible\n",
                static_cast<int>(passes.size()), visibleCount, notVisibleCount);

    TEST_ASSERT_TRUE(visibleCount > 0);
    TEST_ASSERT_TRUE(notVisibleCount > 0);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_iss_can_rise_at_athens);
    RUN_TEST(test_iss_can_rise_just_above_its_inclination);
    RUN_TEST(test_iss_cannot_rise_at_the_north_pole);
    RUN_TEST(test_polar_orbit_can_rise_anywhere);
    RUN_TEST(test_equatorial_orbit_cannot_rise_at_high_latitude);
    RUN_TEST(test_filter_is_symmetric_about_the_equator);
    RUN_TEST(test_iss_has_several_passes_over_athens_in_a_day);
    RUN_TEST(test_passes_are_internally_ordered);
    RUN_TEST(test_passes_are_chronological_and_disjoint);
    RUN_TEST(test_every_pass_clears_the_elevation_floor);
    RUN_TEST(test_passes_last_a_plausible_number_of_minutes);
    RUN_TEST(test_no_passes_where_the_filter_says_impossible);
    RUN_TEST(test_yield_callback_is_invoked);
    RUN_TEST(test_zero_hour_horizon_returns_nothing);
    RUN_TEST(test_rise_and_set_land_on_the_elevation_floor);
    RUN_TEST(test_visible_flag_distinguishes_passes_over_a_week);
    return UNITY_END();
}
