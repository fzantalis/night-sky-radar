#include <unity.h>
#include <cmath>
#include <cstdio>
#include "Propagator.h"
#include "Tle.h"
#include "TimeUtils.h"

// Vallado SGP4-VER.TLE, satellite 00005 (the canonical verification case).
static const char* V5_NAME = "TEST 00005";
static const char* V5_L1 =
    "1 00005U 58002B   00179.78495062  .00000023  00000-0  28098-4 0  4753";
static const char* V5_L2 =
    "2 00005  34.2682 348.7242 1859667 331.7664  19.3264 10.82419157413667";

// Expected TEME position in km at tsince = 0, from tcppver.out.
static const double EXP_X = 7022.46529266;
static const double EXP_Y = -1400.08296755;
static const double EXP_Z = 0.03995155;

static Propagator makeProp(void) {
    Tle t;
    TEST_ASSERT_TRUE(parseTle(V5_NAME, V5_L1, V5_L2, t));
    Propagator p;
    TEST_ASSERT_TRUE(p.init(t));
    return p;
}

static long long unixAtJd(double jd) {
    return static_cast<long long>(
        std::llround((jd - timeutils::JD_UNIX_EPOCH) * 86400.0));
}

void test_init_accepts_valid_tle(void) {
    Tle t;
    TEST_ASSERT_TRUE(parseTle(V5_NAME, V5_L1, V5_L2, t));
    Propagator p;
    TEST_ASSERT_TRUE(p.init(t));
    TEST_ASSERT_TRUE(p.ready());
}

void test_position_at_epoch_matches_reference(void) {
    Propagator p = makeProp();
    Vec3 pos;
    TEST_ASSERT_TRUE(p.positionAt(unixAtJd(p.epochJd()), pos));

    // Tolerance derivation, not a fudge factor. positionAt takes int64 unix
    // SECONDS by design, so reconstructing tsince from a rounded epoch
    // quantises time by up to 0.5 s. Satellite 00005 is eccentric (e=0.186)
    // and near perigee at this epoch, travelling ~8.07 km/s, so worst-case
    // quantisation is ~4 km of real displacement. 5 km covers it.
    //
    // This still pins the propagator hard: a wrong tsince unit, wrong gravity
    // model, or wrong epoch reconstruction is wrong by hundreds to thousands
    // of km, not by metres. Do not tighten this below 5 km, and do not widen
    // it either - if it starts failing, the wrapper broke.
    TEST_ASSERT_DOUBLE_WITHIN(5.0, EXP_X, pos.x);
    TEST_ASSERT_DOUBLE_WITHIN(5.0, EXP_Y, pos.y);
    TEST_ASSERT_DOUBLE_WITHIN(5.0, EXP_Z, pos.z);
}

void test_one_second_of_motion_is_orbital_speed(void) {
    Propagator p = makeProp();
    const long long t0 = unixAtJd(p.epochJd());
    Vec3 a, b;
    TEST_ASSERT_TRUE(p.positionAt(t0, a));
    TEST_ASSERT_TRUE(p.positionAt(t0 + 1, b));
    const double km = (b - a).norm();
    // Near perigee this orbit moves ~8.07 km/s. If tsince were passed in
    // seconds instead of minutes this would be ~0.13 km; in days, absurd.
    TEST_ASSERT_TRUE(km > 6.0);
    TEST_ASSERT_TRUE(km < 10.0);
}

void test_position_is_a_plausible_orbit_radius(void) {
    Propagator p = makeProp();
    Vec3 pos;
    TEST_ASSERT_TRUE(p.positionAt(unixAtJd(p.epochJd()), pos));
    const double r = pos.norm();
    TEST_ASSERT_TRUE(r > 6400.0);
    TEST_ASSERT_TRUE(r < 50000.0);
}

void test_position_changes_over_time(void) {
    Propagator p = makeProp();
    const long long t0 = unixAtJd(p.epochJd());
    Vec3 a, b;
    TEST_ASSERT_TRUE(p.positionAt(t0, a));
    TEST_ASSERT_TRUE(p.positionAt(t0 + 600, b));
    TEST_ASSERT_TRUE((b - a).norm() > 100.0);
}

void test_uninitialised_propagator_refuses_to_propagate(void) {
    Propagator p;
    Vec3 pos{1.0, 2.0, 3.0};
    TEST_ASSERT_FALSE(p.ready());
    TEST_ASSERT_FALSE(p.positionAt(946728000LL, pos));
    // Contract: posKm is left untouched on failure.
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, pos.x);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, pos.y);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, pos.z);
}

void test_init_rejects_garbage(void) {
    Tle t;   // default-constructed, all zeros
    Propagator p;
    TEST_ASSERT_FALSE(p.init(t));
    TEST_ASSERT_FALSE(p.ready());
}

void test_init_does_not_throw_on_garbage(void) {
    // libsgp4 signals bad element sets by throwing. Propagator must absorb that
    // and return false - an exception must never escape to the caller.
    Tle t;
    std::snprintf(t.line1, sizeof(t.line1), "1 not a real tle line at all");
    std::snprintf(t.line2, sizeof(t.line2), "2 also not a real tle line");
    t.satnum = 5;
    Propagator p;
    bool threw = false;
    try {
        TEST_ASSERT_FALSE(p.init(t));
    } catch (...) {
        threw = true;
    }
    TEST_ASSERT_FALSE(threw);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_init_accepts_valid_tle);
    RUN_TEST(test_position_at_epoch_matches_reference);
    RUN_TEST(test_one_second_of_motion_is_orbital_speed);
    RUN_TEST(test_position_is_a_plausible_orbit_radius);
    RUN_TEST(test_position_changes_over_time);
    RUN_TEST(test_uninitialised_propagator_refuses_to_propagate);
    RUN_TEST(test_init_rejects_garbage);
    RUN_TEST(test_init_does_not_throw_on_garbage);
    return UNITY_END();
}
