#include <unity.h>

#include <cmath>
#include <string>
#include <vector>

#include "Neo.h"

namespace {

// A verbatim excerpt of a real response from
// https://ssd-api.jpl.nasa.gov/cad.api?dist-max=10LD&date-max=+30&fullname=true
// retrieved 2026-09-05. Three of the six rows, unedited apart from the
// truncation - including the leading spaces JPL pads `fullname` with, and the
// exact float formatting it emits.
const char* const kRealCad = R"JSON({"signature":{"version":"1.5","source":"NASA/JPL SBDB Close Approach Data API"},"count":3,"fields":["des","orbit_id","jd","cd","dist","dist_min","dist_max","v_rel","v_inf","t_sigma_f","h","fullname"],"data":[["2026 RG","2","2461288.947527590","2026-Sep-05 10:44","0.00339781083711804","0.00338494168802013","0.00341067919090407","12.4031921518038","12.33980652001","00:01","27.56","       (2026 RG)"],["2026 RD","1","2461290.281774406","2026-Sep-06 18:46","0.00697532073383601","0.0068943439895369","0.00705628625137799","5.85099911256097","5.78534504873067","00:24","28.757","       (2026 RD)"],["2024 RV12","4","2461292.629810468","2026-Sep-09 03:07","0.0143017772431881","0.00847783519858677","0.0650840489169311","12.0580966619759","12.0426362132812","4_21:45","26.06","       (2024 RV12)"]]})JSON";

// What the API actually returns when nothing comes close in the window. Note
// what is NOT here: no `fields`, no `data`. Captured 2026-09-05 from
// ?dist-max=0.2LD&date-min=2026-09-05&date-max=2026-09-06.
const char* const kEmptyCad =
    R"JSON({"count":0,"signature":{"version":"1.5","source":"NASA/JPL SBDB Close Approach Data API"}})JSON";

// The API's own error body, served with HTTP 400. Captured from ?dist-max=nonsense.
const char* const kErrorBody =
    R"JSON({"moreInfo":"https://ssd-api.jpl.nasa.gov/doc/cad.html","code":"400","message":"invalid number and/or units"})JSON";

// 2026-09-05T00:00:00Z.
constexpr int64_t kSep5 = 1788566400;

}  // namespace

// ---------------------------------------------------------------- julian date

void test_julian_epoch_is_unix_zero(void) {
    TEST_ASSERT_EQUAL_INT64(0, neo::julianDateToUnix(2440587.5));
}

void test_j2000_julian_date(void) {
    // JD 2451545.0 is 2000-01-01T12:00:00Z by definition.
    TEST_ASSERT_EQUAL_INT64(946728000, neo::julianDateToUnix(2451545.0));
}

// ------------------------------------------------------------------- parsing

void test_parses_a_real_cad_response(void) {
    std::vector<neo::Approach> out;
    TEST_ASSERT_TRUE(neo::parseCad(kRealCad, out, 0));
    TEST_ASSERT_EQUAL(3, static_cast<int>(out.size()));

    TEST_ASSERT_EQUAL_STRING("2026 RG", out[0].des.c_str());
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.00339781083711804, out[0].distAu);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 12.4031921518038, out[0].vRelKmS);
    TEST_ASSERT_TRUE(out[0].hKnown);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 27.56, out[0].hMag);
}

// Cross-check `jd` against the API's own `cd` string. These are two
// independent representations of the same instant in the same row, so if the
// epoch constant in julianDateToUnix() were wrong, or the column index were
// off by one, this disagrees. 2026-Sep-05 10:44 UTC is 38,640 s past midnight;
// jd carries seconds that cd truncates, hence the 90 s tolerance.
void test_parsed_time_agrees_with_the_apis_own_calendar_string(void) {
    std::vector<neo::Approach> out;
    TEST_ASSERT_TRUE(neo::parseCad(kRealCad, out, 0));
    const int64_t expected = kSep5 + 38640;
    TEST_ASSERT_INT64_WITHIN(90, expected, out[0].approachUnix);
}

// The load-bearing one. A quiet month is not a broken fetch: JPL omits
// `fields` and `data` entirely when count is 0, so a parser that demanded
// them would report a perfectly good empty result as a corrupt payload - and
// the caller would be unable to tell that apart from a network fault.
void test_zero_count_response_is_a_success_not_a_failure(void) {
    std::vector<neo::Approach> out;
    TEST_ASSERT_TRUE(neo::parseCad(kEmptyCad, out, 0));
    TEST_ASSERT_EQUAL(0, static_cast<int>(out.size()));
}

void test_error_body_is_rejected(void) {
    std::vector<neo::Approach> out;
    TEST_ASSERT_FALSE(neo::parseCad(kErrorBody, out, 0));
}

void test_html_and_garbage_are_rejected(void) {
    std::vector<neo::Approach> out;
    TEST_ASSERT_FALSE(neo::parseCad("<html><body>captive portal</body></html>", out, 0));
    TEST_ASSERT_FALSE(neo::parseCad("", out, 0));
    TEST_ASSERT_FALSE(neo::parseCad("{\"count\":3}", out, 0));  // count but no fields
}

void test_truncated_body_is_rejected(void) {
    std::string cut(kRealCad);
    cut = cut.substr(0, cut.size() - 40);   // lose the closing brackets
    std::vector<neo::Approach> out;
    TEST_ASSERT_FALSE(neo::parseCad(cut.c_str(), out, 0));
}

// A null `h` must leave hKnown false rather than parsing as 0.0, which would
// render as the brightest possible object.
void test_null_magnitude_is_unknown_not_zero(void) {
    const char* json = R"JSON({"count":1,"fields":["des","jd","dist","h"],
        "data":[["2026 XX","2461288.5","0.0025695552897",null]]})JSON";
    std::vector<neo::Approach> out;
    TEST_ASSERT_TRUE(neo::parseCad(json, out, 0));
    TEST_ASSERT_EQUAL(1, static_cast<int>(out.size()));
    TEST_ASSERT_FALSE(out[0].hKnown);
    TEST_ASSERT_TRUE(out[0].hMag > 90.0);
}

// A null in the middle of a row must not shift the columns after it. Here a
// null v_rel sits between dist and h; if nulls were skipped rather than kept
// as placeholders, h would be read from the wrong column.
void test_null_in_mid_row_does_not_shift_later_columns(void) {
    const char* json = R"JSON({"count":1,"fields":["des","jd","dist","v_rel","h"],
        "data":[["2026 YY","2461288.5","0.0025695552897",null,"22.5"]]})JSON";
    std::vector<neo::Approach> out;
    TEST_ASSERT_TRUE(neo::parseCad(json, out, 0));
    TEST_ASSERT_TRUE(out[0].hKnown);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 22.5, out[0].hMag);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, out[0].vRelKmS);
}

void test_fullname_padding_is_trimmed(void) {
    std::vector<neo::Approach> out;
    TEST_ASSERT_TRUE(neo::parseCad(kRealCad, out, 0));
    TEST_ASSERT_EQUAL_STRING("(2026 RG)", out[0].fullname.c_str());
}

void test_max_out_caps_the_result(void) {
    std::vector<neo::Approach> out;
    TEST_ASSERT_TRUE(neo::parseCad(kRealCad, out, 2));
    TEST_ASSERT_EQUAL(2, static_cast<int>(out.size()));
    // The cap must keep the first rows, not an arbitrary subset.
    TEST_ASSERT_EQUAL_STRING("2026 RG", out[0].des.c_str());
    TEST_ASSERT_EQUAL_STRING("2026 RD", out[1].des.c_str());
}

// ------------------------------------------------------------- lunar distance

// One lunar distance in au, straight from the constant, must come back as
// exactly 1 LD. This pins the AU/LD conversion in both directions at once.
void test_one_lunar_distance_round_trips(void) {
    const char* json = R"JSON({"count":1,"fields":["des","jd","dist"],
        "data":[["1LD","2461288.5","0.00256955528970"]]})JSON";
    std::vector<neo::Approach> out;
    TEST_ASSERT_TRUE(neo::parseCad(json, out, 0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, out[0].distLd);
}

// The Moon is about 384,400 km away and the real 2026 RG pass is about 1.32 LD.
// A conversion inverted (multiplying instead of dividing) would give ~1e-5.
void test_real_approach_distance_in_lunar_distances_is_plausible(void) {
    std::vector<neo::Approach> out;
    TEST_ASSERT_TRUE(neo::parseCad(kRealCad, out, 0));
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1.322, out[0].distLd);
    TEST_ASSERT_TRUE(out[0].distLd > 1.0 && out[0].distLd < 2.0);
}

// ---------------------------------------------------------------- projection

void test_radius_is_distance_over_the_rim(void) {
    neo::Approach a;
    a.distLd = 5.0;
    double r = 0.0, theta = 0.0;
    neo::project(a, 0, 30.0, 10.0, r, theta);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, r);
}

void test_radius_clamps_at_the_rim(void) {
    neo::Approach a;
    a.distLd = 40.0;                       // far beyond the rim
    double r = 0.0, theta = 0.0;
    neo::project(a, 0, 30.0, 10.0, r, theta);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, r);

    a.distLd = 0.0;                        // a direct hit sits at the centre
    neo::project(a, 0, 30.0, 10.0, r, theta);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, r);
}

// Angle encodes *when*, measured from the top and increasing clockwise. These
// three points pin the convention hard enough that an inverted or offset
// mapping fails: a sign flip breaks the quarter case, and an offset breaks now.
void test_angle_encodes_position_in_the_window(void) {
    const int64_t now = kSep5;
    const double  windowDays = 30.0;
    double r = 0.0, theta = 0.0;

    neo::Approach atNow;
    atNow.approachUnix = now;
    neo::project(atNow, now, windowDays, 10.0, r, theta);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, theta);

    neo::Approach quarter;
    quarter.approachUnix = now + static_cast<int64_t>(7.5 * 86400);
    neo::project(quarter, now, windowDays, 10.0, r, theta);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 90.0, theta);

    neo::Approach half;
    half.approachUnix = now + static_cast<int64_t>(15.0 * 86400);
    neo::project(half, now, windowDays, 10.0, r, theta);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 180.0, theta);
}

// Later must mean further round. Stated separately from the fixed points above
// because it is the property a renderer actually depends on.
void test_angle_increases_with_time(void) {
    const int64_t now = kSep5;
    double r = 0.0, prev = -1.0, theta = 0.0;
    for (int day = 0; day < 30; ++day) {
        neo::Approach a;
        a.approachUnix = now + static_cast<int64_t>(day) * 86400;
        neo::project(a, now, 30.0, 10.0, r, theta);
        TEST_ASSERT_TRUE(theta > prev);
        prev = theta;
    }
}

// An approach already past normalises forward instead of going negative, which
// would put it off the dial entirely.
void test_past_approach_normalises_into_range(void) {
    const int64_t now = kSep5;
    neo::Approach a;
    a.approachUnix = now - 86400;          // yesterday
    double r = 0.0, theta = 0.0;
    neo::project(a, now, 30.0, 10.0, r, theta);
    TEST_ASSERT_TRUE(theta >= 0.0 && theta < 360.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 348.0, theta);   // 360 - 12
}

// Degenerate parameters must not produce NaN or a divide-by-zero - this
// project has been bitten by non-finite values reaching the snapshot before.
void test_degenerate_parameters_stay_finite(void) {
    neo::Approach a;
    a.distLd = 3.0;
    a.approachUnix = kSep5 + 86400;
    double r = -1.0, theta = -1.0;
    neo::project(a, kSep5, 0.0, 0.0, r, theta);
    TEST_ASSERT_TRUE(std::isfinite(r));
    TEST_ASSERT_TRUE(std::isfinite(theta));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_julian_epoch_is_unix_zero);
    RUN_TEST(test_j2000_julian_date);
    RUN_TEST(test_parses_a_real_cad_response);
    RUN_TEST(test_parsed_time_agrees_with_the_apis_own_calendar_string);
    RUN_TEST(test_zero_count_response_is_a_success_not_a_failure);
    RUN_TEST(test_error_body_is_rejected);
    RUN_TEST(test_html_and_garbage_are_rejected);
    RUN_TEST(test_truncated_body_is_rejected);
    RUN_TEST(test_null_magnitude_is_unknown_not_zero);
    RUN_TEST(test_null_in_mid_row_does_not_shift_later_columns);
    RUN_TEST(test_fullname_padding_is_trimmed);
    RUN_TEST(test_max_out_caps_the_result);
    RUN_TEST(test_one_lunar_distance_round_trips);
    RUN_TEST(test_real_approach_distance_in_lunar_distances_is_plausible);
    RUN_TEST(test_radius_is_distance_over_the_rim);
    RUN_TEST(test_radius_clamps_at_the_rim);
    RUN_TEST(test_angle_encodes_position_in_the_window);
    RUN_TEST(test_angle_increases_with_time);
    RUN_TEST(test_past_approach_normalises_into_range);
    RUN_TEST(test_degenerate_parameters_stay_finite);
    return UNITY_END();
}
