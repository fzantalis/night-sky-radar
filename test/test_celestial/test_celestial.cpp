#include <unity.h>
#include <cmath>

#include "Celestial.h"
#include "Observer.h"
#include "TimeUtils.h"

// This file pins radiantLookAngles' convention from both ends, the way
// test_magnitude's backlit/fully-lit pair pins phaseAngleRad. Each test below
// uses a symmetry argument that cannot mirror the implementation's own
// algebra - if it can, an inverted convention would pass it too, and this
// project has shipped exactly that bug once already (see Magnitude.cpp's
// phase-angle test comments).

namespace {
constexpr double PI = 3.14159265358979323846;

// Azimuth lives on a circle: 359.9999999 and 0.0000001 are the same
// direction. A raw TEST_ASSERT_DOUBLE_WITHIN at the exact 0/360 boundary
// would flag that as a ~360-degree error, so wrap the difference into
// [0, 180] before comparing - this is about the wraparound, not about
// forgiving any real convention error (a genuine +/-90 or +/-180 mistake
// still fails every one of these assertions).
void assertAzimuthNear(double expectedDeg, double actualDeg, double tolDeg) {
    double diff = std::fmod(std::fabs(actualDeg - expectedDeg), 360.0);
    if (diff > 180.0) diff = 360.0 - diff;
    TEST_ASSERT_TRUE(diff <= tolDeg);
}
}

// --- 1. Zenith: dec = lat, hour angle 0 -> elevation 90, for any lon/time. --

void test_object_at_zenith_has_elevation_90_regardless_of_longitude(void) {
    const int64_t times[] = {0, 1000000000LL, 1893456789LL};
    const double  lons[]  = {-150.0, 0.0, 23.73, 179.0};
    const double  lat     = 37.98;

    for (int64_t t : times) {
        for (double lon : lons) {
            Observer obs{lat, lon, 0.0};
            // Hour angle 0 means RA equals the local sidereal time at this
            // instant - construct it from the same gmstDegrees the production
            // code calls, so this is "for any real instant", not a contrived
            // single case.
            const double gmst = timeutils::gmstDegrees(timeutils::julianDate(t));
            double ra = gmst + lon;
            ra = std::fmod(ra, 360.0);
            if (ra < 0.0) ra += 360.0;

            LookAngles la = radiantLookAngles(ra, lat, obs, t);
            TEST_ASSERT_DOUBLE_WITHIN(0.01, 90.0, la.elDeg);
        }
    }
}

// --- 2. Celestial pole: due north at elevation = latitude, any time/lon. ---
// Pure symmetry, no hour-angle algebra: the north celestial pole (dec=+90)
// sits on Earth's rotation axis, so it cannot move in the sky as the Earth
// turns - its look angles are the same at every hour angle.

void test_celestial_pole_is_due_north_at_elevation_equal_to_latitude(void) {
    const double lats[] = {10.0, 37.98, 60.0, 89.0};
    const double ras[]  = {0.0, 90.0, 200.0, 359.0};   // RA must not matter
    const int64_t times[] = {0, 500000000LL, 1893456789LL};

    for (double lat : lats) {
        Observer obs{lat, 23.73, 0.0};
        for (double ra : ras) {
            for (int64_t t : times) {
                LookAngles la = radiantLookAngles(ra, 90.0, obs, t);
                assertAzimuthNear(0.0, la.azDeg, 1e-6);
                TEST_ASSERT_DOUBLE_WITHIN(0.01, lat, la.elDeg);
            }
        }
    }
}

// --- 3. Same pole, southern hemisphere: below the horizon. -----------------

void test_celestial_pole_is_below_horizon_from_southern_hemisphere(void) {
    Observer obs{-37.98, 23.73, 0.0};
    LookAngles la = radiantLookAngles(120.0, 90.0, obs, 1700000000LL);
    TEST_ASSERT_TRUE(la.elDeg < 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, -37.98, la.elDeg);
}

// --- 4. Azimuth east of north while rising, west of north while setting. ---
// Before meridian transit an object is on the eastern side of the sky; after
// transit it is on the western side. This is a basic fact of the geometry,
// independent of how the azimuth formula itself is coded.

void test_azimuth_is_east_of_north_before_transit_and_west_after(void) {
    const double lat = 37.98;
    const double lon = 23.73;
    const int64_t t  = 1800000000LL;
    Observer obs{lat, lon, 0.0};

    const double gmst = timeutils::gmstDegrees(timeutils::julianDate(t));
    double lst = std::fmod(gmst + lon, 360.0);
    if (lst < 0.0) lst += 360.0;

    // H = LST - RA. Choosing RA = LST + 30 gives H = -30 (still to transit,
    // i.e. currently rising, east side). RA = LST - 30 gives H = +30 (past
    // transit, setting, west side).
    auto norm360 = [](double d) {
        d = std::fmod(d, 360.0);
        if (d < 0.0) d += 360.0;
        return d;
    };
    const double raRising  = norm360(lst + 30.0);
    const double raSetting = norm360(lst - 30.0);

    LookAngles rising  = radiantLookAngles(raRising, 0.0, obs, t);
    LookAngles setting = radiantLookAngles(raSetting, 0.0, obs, t);

    TEST_ASSERT_TRUE(rising.azDeg > 0.0 && rising.azDeg < 180.0);
    TEST_ASSERT_TRUE(setting.azDeg > 180.0 && setting.azDeg < 360.0);
}

// --- Housekeeping: clamp/finiteness and range normalisation. ---------------

void test_elevation_is_finite_at_the_zenith_sweep(void) {
    // Same rationale as Topocentric's zenith sweep test: sinEl can round to
    // just over 1.0 right at the boundary the clamp exists for. Sweep dec
    // across [-90, 90] and lat across a spread of values at hour angle 0.
    const double lats[] = {-89.0, -45.0, 0.0, 45.0, 89.0};
    for (double lat : lats) {
        Observer obs{lat, 12.3, 0.0};
        for (double dec = -90.0; dec <= 90.0; dec += 5.0) {
            const int64_t t = 1700000000LL;
            const double gmst = timeutils::gmstDegrees(timeutils::julianDate(t));
            double ra = std::fmod(gmst + obs.lonDeg, 360.0);
            if (ra < 0.0) ra += 360.0;
            LookAngles la = radiantLookAngles(ra, dec, obs, t);
            TEST_ASSERT_TRUE(std::isfinite(la.elDeg));
            TEST_ASSERT_TRUE(la.elDeg >= -90.0001 && la.elDeg <= 90.0001);
        }
    }
}

void test_azimuth_always_in_range(void) {
    Observer obs{37.98, 23.73, 0.015};
    for (double ra = 0.0; ra < 360.0; ra += 17.0) {
        for (double dec = -80.0; dec <= 80.0; dec += 20.0) {
            LookAngles la = radiantLookAngles(ra, dec, obs, 1700000000LL);
            TEST_ASSERT_TRUE(la.azDeg >= 0.0);
            TEST_ASSERT_TRUE(la.azDeg < 360.0);
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_object_at_zenith_has_elevation_90_regardless_of_longitude);
    RUN_TEST(test_celestial_pole_is_due_north_at_elevation_equal_to_latitude);
    RUN_TEST(test_celestial_pole_is_below_horizon_from_southern_hemisphere);
    RUN_TEST(test_azimuth_is_east_of_north_before_transit_and_west_after);
    RUN_TEST(test_elevation_is_finite_at_the_zenith_sweep);
    RUN_TEST(test_azimuth_always_in_range);
    return UNITY_END();
}
