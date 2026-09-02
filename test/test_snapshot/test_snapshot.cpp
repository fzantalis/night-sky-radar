#include <unity.h>
#include <string>
#include "Snapshot.h"

static bool contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

static Blip makeBlip(int id, const char* name, double r, bool vis) {
    Blip b;
    b.id = id;
    b.name = name;
    b.r = r;
    b.theta = 142.3;
    b.magnitude = -3.1;
    b.visible = vis;
    return b;
}

void test_json_reports_timestamp_and_status(void) {
    Snapshot s;
    s.t = 1756800000LL;
    s.status = ScopeStatus::Ok;
    s.tleAgeHours = 6.25;
    const std::string j = toJson(s);
    TEST_ASSERT_TRUE(contains(j, "\"t\":1756800000"));
    TEST_ASSERT_TRUE(contains(j, "\"status\":\"ok\""));
    TEST_ASSERT_TRUE(contains(j, "\"tleAgeHours\":6.25"));
}

void test_json_status_no_time(void) {
    Snapshot s;
    s.status = ScopeStatus::NoTime;
    TEST_ASSERT_TRUE(contains(toJson(s), "\"status\":\"no_time\""));
}

void test_json_status_no_location(void) {
    Snapshot s;
    s.status = ScopeStatus::NoLocation;
    TEST_ASSERT_TRUE(contains(toJson(s), "\"status\":\"no_location\""));
}

void test_json_status_offline(void) {
    Snapshot s;
    s.status = ScopeStatus::Offline;
    TEST_ASSERT_TRUE(contains(toJson(s), "\"status\":\"offline\""));
}

void test_json_emits_blip_fields(void) {
    Snapshot s;
    s.status = ScopeStatus::Ok;
    s.blips.push_back(makeBlip(25544, "ISS (ZARYA)", 0.58, true));
    const std::string j = toJson(s);
    TEST_ASSERT_TRUE(contains(j, "\"id\":25544"));
    TEST_ASSERT_TRUE(contains(j, "\"name\":\"ISS (ZARYA)\""));
    TEST_ASSERT_TRUE(contains(j, "\"visible\":true"));
}

void test_json_empty_blips_is_valid_array(void) {
    Snapshot s;
    s.status = ScopeStatus::Ok;
    TEST_ASSERT_TRUE(contains(toJson(s), "\"blips\":[]"));
}

void test_json_escapes_quotes_in_names(void) {
    Snapshot s;
    s.status = ScopeStatus::Ok;
    s.blips.push_back(makeBlip(1, "BAD\"NAME", 0.5, false));
    const std::string j = toJson(s);
    TEST_ASSERT_TRUE(contains(j, "BAD\\\"NAME"));
}

void test_rank_puts_visible_objects_first(void) {
    Snapshot s;
    s.blips.push_back(makeBlip(1, "DIM", 0.2, false));
    s.blips.push_back(makeBlip(2, "BRIGHT", 0.9, true));
    rankAndCap(s);
    TEST_ASSERT_EQUAL_INT(2, s.blips[0].id);
}

void test_rank_orders_by_elevation_within_a_group(void) {
    Snapshot s;
    // Smaller r means higher in the sky, so it should sort first.
    s.blips.push_back(makeBlip(1, "LOW",  0.9, true));
    s.blips.push_back(makeBlip(2, "HIGH", 0.1, true));
    rankAndCap(s);
    TEST_ASSERT_EQUAL_INT(2, s.blips[0].id);
}

void test_rank_caps_at_max_blips(void) {
    Snapshot s;
    for (int i = 0; i < 40; ++i) {
        s.blips.push_back(makeBlip(i, "OBJ", 0.5, false));
    }
    rankAndCap(s);
    TEST_ASSERT_EQUAL_INT(MAX_BLIPS, static_cast<int>(s.blips.size()));
}

void test_rank_leaves_short_lists_alone(void) {
    Snapshot s;
    s.blips.push_back(makeBlip(1, "A", 0.5, true));
    s.blips.push_back(makeBlip(2, "B", 0.6, true));
    rankAndCap(s);
    TEST_ASSERT_EQUAL_INT(2, static_cast<int>(s.blips.size()));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_json_reports_timestamp_and_status);
    RUN_TEST(test_json_status_no_time);
    RUN_TEST(test_json_status_no_location);
    RUN_TEST(test_json_status_offline);
    RUN_TEST(test_json_emits_blip_fields);
    RUN_TEST(test_json_empty_blips_is_valid_array);
    RUN_TEST(test_json_escapes_quotes_in_names);
    RUN_TEST(test_rank_puts_visible_objects_first);
    RUN_TEST(test_rank_orders_by_elevation_within_a_group);
    RUN_TEST(test_rank_caps_at_max_blips);
    RUN_TEST(test_rank_leaves_short_lists_alone);
    return UNITY_END();
}
