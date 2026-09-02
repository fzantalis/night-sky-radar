#include <unity.h>
#include <cstring>
#include "Tle.h"

// A real ISS element set. The trailing digit of each line is its checksum.
static const char* ISS_NAME = "ISS (ZARYA)";
static const char* ISS_L1 =
    "1 25544U 98067A   24001.50000000  .00016717  00000-0  30074-3 0  9991";
static const char* ISS_L2 =
    "2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537";

void test_parses_satellite_number(void) {
    Tle t;
    TEST_ASSERT_TRUE(parseTle(ISS_NAME, ISS_L1, ISS_L2, t));
    TEST_ASSERT_EQUAL_INT(25544, t.satnum);
}

void test_parses_name(void) {
    Tle t;
    TEST_ASSERT_TRUE(parseTle(ISS_NAME, ISS_L1, ISS_L2, t));
    TEST_ASSERT_EQUAL_STRING("ISS (ZARYA)", t.name);
}

// International designator 98067A -> launched 1998, launch number 67.
// Two-digit years below 57 mean 20xx; 57 and above mean 19xx.
void test_parses_cospar_designator_1998(void) {
    Tle t;
    TEST_ASSERT_TRUE(parseTle(ISS_NAME, ISS_L1, ISS_L2, t));
    TEST_ASSERT_EQUAL_INT(1998, t.launchYear);
    TEST_ASSERT_EQUAL_INT(67, t.launchNumber);
}

// Editing the designator invalidates the checksum, so the parse must reject it.
// This proves the checksum guard runs before any field is trusted.
//
// The 20xx branch of the year window is not covered here because fabricating a
// TLE with a valid checksum by hand is error-prone. It is exercised for real at
// M3, where every Starlink element set has a 20xx designator; if the branch were
// wrong, the Starlink train filter would select nothing.
void test_designator_edit_invalidates_checksum(void) {
    Tle t;
    char l1[70];
    std::strcpy(l1, ISS_L1);
    l1[9]  = '2'; l1[10] = '4';                // columns 10-11: launch year
    l1[11] = '1'; l1[12] = '2'; l1[13] = '3';  // columns 12-14: launch number
    TEST_ASSERT_FALSE(parseTle(ISS_NAME, l1, ISS_L2, t));
}

void test_parses_inclination_and_mean_motion(void) {
    Tle t;
    TEST_ASSERT_TRUE(parseTle(ISS_NAME, ISS_L1, ISS_L2, t));
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 51.6416, t.inclinationDeg);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 15.72125391, t.meanMotionRevPerDay);
}

void test_retains_raw_lines_for_sgp4(void) {
    Tle t;
    TEST_ASSERT_TRUE(parseTle(ISS_NAME, ISS_L1, ISS_L2, t));
    TEST_ASSERT_EQUAL_STRING(ISS_L1, t.line1);
    TEST_ASSERT_EQUAL_STRING(ISS_L2, t.line2);
}

void test_valid_checksums_accepted(void) {
    TEST_ASSERT_TRUE(tleChecksumValid(ISS_L1));
    TEST_ASSERT_TRUE(tleChecksumValid(ISS_L2));
}

void test_corrupted_line_rejected(void) {
    char bad[70];
    std::strcpy(bad, ISS_L1);
    bad[20] = (bad[20] == '9') ? '8' : '9';  // perturb a digit
    TEST_ASSERT_FALSE(tleChecksumValid(bad));

    Tle t;
    TEST_ASSERT_FALSE(parseTle(ISS_NAME, bad, ISS_L2, t));
}

void test_wrong_line_numbers_rejected(void) {
    Tle t;
    // Passing line 2 where line 1 is expected must fail.
    TEST_ASSERT_FALSE(parseTle(ISS_NAME, ISS_L2, ISS_L2, t));
}

void test_short_line_rejected(void) {
    Tle t;
    TEST_ASSERT_FALSE(parseTle(ISS_NAME, "1 25544U", ISS_L2, t));
}

void test_mismatched_satnums_rejected(void) {
    Tle t;
    char l2[70];
    std::strcpy(l2, ISS_L2);
    l2[6] = '3';  // change 25544 -> 25543 on line 2 only
    TEST_ASSERT_FALSE(parseTle(ISS_NAME, ISS_L1, l2, t));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_satellite_number);
    RUN_TEST(test_parses_name);
    RUN_TEST(test_parses_cospar_designator_1998);
    RUN_TEST(test_designator_edit_invalidates_checksum);
    RUN_TEST(test_parses_inclination_and_mean_motion);
    RUN_TEST(test_retains_raw_lines_for_sgp4);
    RUN_TEST(test_valid_checksums_accepted);
    RUN_TEST(test_corrupted_line_rejected);
    RUN_TEST(test_wrong_line_numbers_rejected);
    RUN_TEST(test_short_line_rejected);
    RUN_TEST(test_mismatched_satnums_rejected);
    return UNITY_END();
}
