#include <unity.h>
#include "TleCount.h"

using tlecount::tleCountIsPlausible;

void test_zero_count_always_rejected(void) {
    TEST_ASSERT_FALSE(tleCountIsPlausible(0, 0));
    TEST_ASSERT_FALSE(tleCountIsPlausible(0, 21));
    TEST_ASSERT_FALSE(tleCountIsPlausible(0, 175));
}

void test_any_count_accepted_when_no_history(void) {
    TEST_ASSERT_TRUE(tleCountIsPlausible(1, 0));
    TEST_ASSERT_TRUE(tleCountIsPlausible(21, 0));
    TEST_ASSERT_TRUE(tleCountIsPlausible(175, 0));
}

void test_count_equal_to_previous_accepted(void) {
    TEST_ASSERT_TRUE(tleCountIsPlausible(21, 21));
    TEST_ASSERT_TRUE(tleCountIsPlausible(175, 175));
}

void test_count_just_over_half_accepted(void) {
    // previousCount=175: half is 87.5, so 88 (2*88=176 >= 175) must pass.
    TEST_ASSERT_TRUE(tleCountIsPlausible(88, 175));
}

void test_count_just_under_half_rejected(void) {
    // previousCount=175: 87 (2*87=174 < 175) must fail.
    TEST_ASSERT_FALSE(tleCountIsPlausible(87, 175));
}

// The regression this whole file exists to prevent: a small group (e.g.
// "stations", ~21 objects) validated against its own history must never be
// rejected just because some *other* group's history is much larger.
void test_small_group_against_its_own_history_accepted(void) {
    TEST_ASSERT_TRUE(tleCountIsPlausible(21, 21));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_count_always_rejected);
    RUN_TEST(test_any_count_accepted_when_no_history);
    RUN_TEST(test_count_equal_to_previous_accepted);
    RUN_TEST(test_count_just_over_half_accepted);
    RUN_TEST(test_count_just_under_half_rejected);
    RUN_TEST(test_small_group_against_its_own_history_accepted);
    return UNITY_END();
}
