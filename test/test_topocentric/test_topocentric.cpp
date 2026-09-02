#include <unity.h>
#include <cmath>
#include "Topocentric.h"
#include "Observer.h"
#include "Vec3.h"

static constexpr double RE_KM = 6378.137;

void test_site_at_equator_prime_meridian_zero_gmst(void) {
    // At GMST 0 the prime meridian aligns with the ECI x-axis, so a sea-level
    // observer at (0N, 0E) sits on the x-axis at one Earth radius.
    Observer obs{0.0, 0.0, 0.0};
    Vec3 s = siteEci(obs, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, RE_KM, s.x);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, s.y);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, s.z);
}

void test_site_rotates_with_gmst(void) {
    // 90 degrees of GMST swings the same observer onto the y-axis.
    Observer obs{0.0, 0.0, 0.0};
    Vec3 s = siteEci(obs, 90.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, s.x);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, RE_KM, s.y);
}

void test_site_at_north_pole_is_on_z_axis(void) {
    // WGS84 polar radius is about 6356.752 km.
    Observer obs{90.0, 0.0, 0.0};
    Vec3 s = siteEci(obs, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.0, s.x);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.0, s.y);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 6356.752, s.z);
}

void test_altitude_raises_the_site(void) {
    Observer sea{0.0, 0.0, 0.0};
    Observer high{0.0, 0.0, 10.0};
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 10.0,
        siteEci(high, 0.0).norm() - siteEci(sea, 0.0).norm());
}

void test_object_directly_overhead_has_elevation_90(void) {
    Observer obs{0.0, 0.0, 0.0};
    // 400 km straight up from the site along the same radial direction.
    Vec3 site = siteEci(obs, 0.0);
    Vec3 sat  = site.unit() * (site.norm() + 400.0);
    LookAngles la = look(sat, obs, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 90.0, la.elDeg);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 400.0, la.rangeKm);
}

void test_object_below_the_site_has_negative_elevation(void) {
    Observer obs{0.0, 0.0, 0.0};
    Vec3 site = siteEci(obs, 0.0);
    Vec3 sat  = site.unit() * (site.norm() - 400.0);  // underground
    LookAngles la = look(sat, obs, 0.0);
    TEST_ASSERT_TRUE(la.elDeg < -80.0);
}

void test_azimuth_of_object_toward_north(void) {
    // From (0N, 0E) at GMST 0, displacing the overhead point toward +z (north)
    // must give an azimuth near 0 degrees.
    Observer obs{0.0, 0.0, 0.0};
    Vec3 site = siteEci(obs, 0.0);
    Vec3 sat  = site.unit() * (site.norm() + 400.0) + Vec3{0.0, 0.0, 200.0};
    LookAngles la = look(sat, obs, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 0.0, la.azDeg);
}

void test_azimuth_of_object_toward_east(void) {
    // Displacing toward +y (east, at GMST 0 and lon 0) must give ~90 degrees.
    Observer obs{0.0, 0.0, 0.0};
    Vec3 site = siteEci(obs, 0.0);
    Vec3 sat  = site.unit() * (site.norm() + 400.0) + Vec3{0.0, 200.0, 0.0};
    LookAngles la = look(sat, obs, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 90.0, la.azDeg);
}

void test_azimuth_of_object_toward_south(void) {
    Observer obs{0.0, 0.0, 0.0};
    Vec3 site = siteEci(obs, 0.0);
    Vec3 sat  = site.unit() * (site.norm() + 400.0) + Vec3{0.0, 0.0, -200.0};
    LookAngles la = look(sat, obs, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 180.0, la.azDeg);
}

void test_azimuth_of_object_toward_west(void) {
    Observer obs{0.0, 0.0, 0.0};
    Vec3 site = siteEci(obs, 0.0);
    Vec3 sat  = site.unit() * (site.norm() + 400.0) + Vec3{0.0, -200.0, 0.0};
    LookAngles la = look(sat, obs, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 270.0, la.azDeg);
}

void test_azimuth_always_in_range(void) {
    Observer obs{37.98, 23.73, 0.1};
    for (double g = 0.0; g < 360.0; g += 17.0) {
        Vec3 site = siteEci(obs, g);
        Vec3 sat  = site.unit() * (site.norm() + 500.0) + Vec3{100.0, -60.0, 30.0};
        LookAngles la = look(sat, obs, g);
        TEST_ASSERT_TRUE(la.azDeg >= 0.0);
        TEST_ASSERT_TRUE(la.azDeg < 360.0);
    }
}

void test_object_over_north_pole_is_due_north_from_midlatitude(void) {
    // Observer at 45N, 0E. Satellite at geographic north pole.
    // By symmetry, this satellite is due north from any northern-hemisphere observer.
    // Azimuth must be 0 (or very close, since rE should be exactly zero here).
    Observer obs{45.0, 0.0, 0.0};
    Vec3 sat{0.0, 0.0, 30000.0};  // over the north pole
    LookAngles la = look(sat, obs, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, la.azDeg);
    // Elevation should be positive and below 90 (roughly 35 degrees at this geometry)
    TEST_ASSERT_TRUE(la.elDeg > 20.0 && la.elDeg < 50.0);
}

void test_object_over_equator_at_same_longitude_is_due_south_from_north(void) {
    // Observer at 45N, 0E. Satellite at geostationary radius over equator at lon 0E.
    // By symmetry, this is due south from the observer.
    Observer obs{45.0, 0.0, 0.0};
    Vec3 sat{42164.0, 0.0, 0.0};  // geostationary radius, over equator at observer's longitude
    LookAngles la = look(sat, obs, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 180.0, la.azDeg);
    // Elevation should be positive (well above horizon)
    TEST_ASSERT_TRUE(la.elDeg > 0.0);
}

void test_object_over_north_pole_is_still_due_north_from_southern_hemisphere(void) {
    // Observer at 45S, 0E. Satellite at geographic north pole.
    // By symmetry, the north pole is still due north (azimuth 0) even from southern hemisphere.
    // This catches sign errors that are symmetric about the equator.
    // From southern hemisphere, the north celestial pole is below the horizon.
    Observer obs{-45.0, 0.0, 0.0};
    Vec3 sat{0.0, 0.0, 30000.0};
    LookAngles la = look(sat, obs, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, la.azDeg);
    TEST_ASSERT_TRUE(la.elDeg < 0.0);  // below horizon for southern observer
}

void test_object_over_south_pole_is_due_south_from_northern_hemisphere(void) {
    // Observer at 45N, 0E. Satellite at geographic south pole.
    // By symmetry, the south pole is due south (azimuth 180) from a northern observer.
    Observer obs{45.0, 0.0, 0.0};
    Vec3 sat{0.0, 0.0, -30000.0};  // over the south pole
    LookAngles la = look(sat, obs, 0.0);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 180.0, la.azDeg);
    TEST_ASSERT_TRUE(la.elDeg < 0.0);  // below horizon
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_site_at_equator_prime_meridian_zero_gmst);
    RUN_TEST(test_site_rotates_with_gmst);
    RUN_TEST(test_site_at_north_pole_is_on_z_axis);
    RUN_TEST(test_altitude_raises_the_site);
    RUN_TEST(test_object_directly_overhead_has_elevation_90);
    RUN_TEST(test_object_below_the_site_has_negative_elevation);
    RUN_TEST(test_azimuth_of_object_toward_north);
    RUN_TEST(test_azimuth_of_object_toward_east);
    RUN_TEST(test_azimuth_of_object_toward_south);
    RUN_TEST(test_azimuth_of_object_toward_west);
    RUN_TEST(test_azimuth_always_in_range);
    RUN_TEST(test_object_over_north_pole_is_due_north_from_midlatitude);
    RUN_TEST(test_object_over_equator_at_same_longitude_is_due_south_from_north);
    RUN_TEST(test_object_over_north_pole_is_still_due_north_from_southern_hemisphere);
    RUN_TEST(test_object_over_south_pole_is_due_south_from_northern_hemisphere);
    return UNITY_END();
}
