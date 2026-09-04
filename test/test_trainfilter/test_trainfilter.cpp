#include <unity.h>
#include <cstring>
#include "Tle.h"
#include "TrainFilter.h"

using trainfilter::isTrainCandidate;
using trainfilter::Params;

namespace {

Tle makeTle(const char* name, int launchYear, double meanMotion) {
    Tle t;
    std::strncpy(t.name, name, sizeof(t.name) - 1);
    t.launchYear = launchYear;
    t.meanMotionRevPerDay = meanMotion;
    return t;
}

}  // namespace

// --- Mean motion gate ---

void test_rejects_below_threshold_even_with_current_year(void) {
    Tle t = makeTle("STARLINK-9001", 2026, 15.30);
    TEST_ASSERT_FALSE(isTrainCandidate(t, 2026));
}

void test_accepts_exactly_at_threshold(void) {
    Tle t = makeTle("STARLINK-9002", 2026, 15.35);
    TEST_ASSERT_TRUE(isTrainCandidate(t, 2026));
}

void test_accepts_well_above_threshold(void) {
    Tle t = makeTle("STARLINK-9003", 2026, 15.75);
    TEST_ASSERT_TRUE(isTrainCandidate(t, 2026));
}

// --- Year window: current year or the previous one, never derived from a
// date the designator does not carry. ---

void test_accepts_current_year(void) {
    Tle t = makeTle("STARLINK-9004", 2026, 15.75);
    TEST_ASSERT_TRUE(isTrainCandidate(t, 2026));
}

void test_accepts_previous_year(void) {
    Tle t = makeTle("STARLINK-9005", 2025, 15.75);
    TEST_ASSERT_TRUE(isTrainCandidate(t, 2026));
}

void test_rejects_two_years_back_even_with_high_mean_motion(void) {
    // A high mean motion two years back is not a fresh launch - it is some
    // other anomaly (decay, maneuver). The filter must not mistake it for one.
    Tle t = makeTle("STARLINK-9006", 2024, 15.75);
    TEST_ASSERT_FALSE(isTrainCandidate(t, 2026));
}

void test_rejects_year_after_current(void) {
    // Should never occur from a real feed, but the window is exactly
    // [currentYear-1, currentYear] - nothing later.
    Tle t = makeTle("STARLINK-9007", 2027, 15.75);
    TEST_ASSERT_FALSE(isTrainCandidate(t, 2026));
}

// --- DTC exclusion ---
//
// Verified against a live CelesTrak `starlink` fetch on 2026-09-04: ~280
// "Direct to Cell" objects across 15+ launches (Jan-Sep 2026-09-04) sit
// permanently at 15.70-15.71 rev/day - well past the mean-motion bar - not
// because they are freshly launched, but because DTC satellites fly lower
// than the rest of the constellation by design. Without this exclusion those
// months-old objects (lower catalog numbers, so earlier in any streamed
// fetch) would exhaust a caller's maxKeep cap before the fetch ever reached
// that day's actual train, whose members carried the highest catalog numbers
// in the whole group.

void test_rejects_dtc_tagged_object_despite_matching_mean_motion_and_year(void) {
    Tle t = makeTle("STARLINK-11557 [DTC]", 2025, 15.7117);
    TEST_ASSERT_FALSE(isTrainCandidate(t, 2026));
}

void test_accepts_non_dtc_object_with_same_profile(void) {
    Tle t = makeTle("STARLINK-34617", 2025, 15.7525);
    TEST_ASSERT_TRUE(isTrainCandidate(t, 2026));
}

// --- Custom threshold ---

void test_custom_threshold_is_honoured(void) {
    Tle t = makeTle("STARLINK-9008", 2026, 15.40);
    Params p;
    p.minMeanMotionRevPerDay = 15.5;
    TEST_ASSERT_FALSE(isTrainCandidate(t, 2026, p));

    p.minMeanMotionRevPerDay = 15.3;
    TEST_ASSERT_TRUE(isTrainCandidate(t, 2026, p));
}

// --- Real-data regression: pulled live from CelesTrak on 2026-09-04 (see
// docs/superpowers/reports/2026-09-04-m3-report.md). These three pin the
// filter against actual objects rather than only synthetic ones. ---

void test_real_fresh_train_member_accepted(void) {
    // STARLINK-38024, designator 2026-159A, one of 24 members of the single
    // launch that was the only genuine train in that fetch.
    Tle t = makeTle("STARLINK-38024", 2026, 15.75256);
    TEST_ASSERT_TRUE(isTrainCandidate(t, 2026));
}

void test_real_dtc_straggler_rejected(void) {
    // STARLINK-11694 [DTC], designator 2025-119L - high mean motion, in the
    // year window, but permanently-low by design rather than freshly raised.
    Tle t = makeTle("STARLINK-11694 [DTC]", 2025, 15.93259);
    TEST_ASSERT_FALSE(isTrainCandidate(t, 2026));
}

void test_real_operational_bulk_member_rejected(void) {
    // STARLINK-1008, launched 2019, long-settled at the operational cluster's
    // median mean motion.
    Tle t = makeTle("STARLINK-1008", 2019, 15.63347);
    TEST_ASSERT_FALSE(isTrainCandidate(t, 2026));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rejects_below_threshold_even_with_current_year);
    RUN_TEST(test_accepts_exactly_at_threshold);
    RUN_TEST(test_accepts_well_above_threshold);
    RUN_TEST(test_accepts_current_year);
    RUN_TEST(test_accepts_previous_year);
    RUN_TEST(test_rejects_two_years_back_even_with_high_mean_motion);
    RUN_TEST(test_rejects_year_after_current);
    RUN_TEST(test_rejects_dtc_tagged_object_despite_matching_mean_motion_and_year);
    RUN_TEST(test_accepts_non_dtc_object_with_same_profile);
    RUN_TEST(test_custom_threshold_is_honoured);
    RUN_TEST(test_real_fresh_train_member_accepted);
    RUN_TEST(test_real_dtc_straggler_rejected);
    RUN_TEST(test_real_operational_bulk_member_rejected);
    return UNITY_END();
}
