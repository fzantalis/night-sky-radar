#include <unity.h>
#include <string>
#include "Snapshot.h"

static bool contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

// Catches the malformed-structure bugs substring matching cannot see.
static bool wellFormed(const std::string& j) {
    if (j.find(",]") != std::string::npos) return false;
    if (j.find(",}") != std::string::npos) return false;
    if (j.find("[,") != std::string::npos) return false;
    if (j.find("{,") != std::string::npos) return false;
    if (j.find(",,") != std::string::npos) return false;
    int curly = 0, square = 0;
    bool inStr = false, esc = false;
    for (char c : j) {
        if (esc) { esc = false; continue; }
        if (inStr) {
            if (c == '\\') esc = true;
            else if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') inStr = true;
        else if (c == '{') curly++;
        else if (c == '}') curly--;
        else if (c == '[') square++;
        else if (c == ']') square--;
        if (curly < 0 || square < 0) return false;
    }
    return curly == 0 && square == 0 && !inStr;
}

static Blip makeBlip(int id, const char* name, double r, bool vis,
                      double elevationDeg = 45.0) {
    Blip b;
    b.id = id;
    b.name = name;
    b.r = r;
    b.theta = 142.3;
    b.elevationDeg = elevationDeg;
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

void test_json_emits_elevation_field(void) {
    Snapshot s;
    s.status = ScopeStatus::Ok;
    s.blips.push_back(makeBlip(25544, "ISS (ZARYA)", 0.58, true, 37.5));
    const std::string j = toJson(s);
    TEST_ASSERT_TRUE(contains(j, "\"el\":37.50"));
}

void test_json_emits_rings_array(void) {
    // A freshly-constructed Snapshot carries the core's default elevation
    // rings (60/30 grid, 10 floor, 0 horizon) - this exercises the
    // multi-element rings array through the existing wellFormed() structural
    // check, and pins the two kinds a renderer needs to tell apart.
    Snapshot s;
    s.status = ScopeStatus::Ok;
    TEST_ASSERT_TRUE(s.rings.size() >= 4);

    const std::string j = toJson(s);
    TEST_ASSERT_TRUE(wellFormed(j));
    TEST_ASSERT_TRUE(contains(j, "\"rings\":["));
    TEST_ASSERT_TRUE(contains(j, "\"el\":60"));
    TEST_ASSERT_TRUE(contains(j, "\"el\":30"));
    TEST_ASSERT_TRUE(contains(j, "\"kind\":\"floor\""));
    TEST_ASSERT_TRUE(contains(j, "\"kind\":\"horizon\""));
}

void test_json_empty_snapshot_is_well_formed(void) {
    Snapshot s;
    s.status = ScopeStatus::Ok;
    TEST_ASSERT_TRUE(wellFormed(toJson(s)));
}

void test_json_single_blip_is_well_formed(void) {
    Snapshot s;
    s.status = ScopeStatus::Ok;
    s.blips.push_back(makeBlip(25544, "ISS (ZARYA)", 0.58, true));
    TEST_ASSERT_TRUE(wellFormed(toJson(s)));
}

void test_json_with_multiple_blips_and_trails(void) {
    Snapshot s;
    s.status = ScopeStatus::Ok;

    Blip a = makeBlip(1, "ALPHA", 0.1, true);
    a.trail = {{0.10, 10.0}, {0.11, 11.0}, {0.12, 12.0}};

    Blip b = makeBlip(2, "BRAVO", 0.2, true);
    b.trail = {{0.20, 20.0}, {0.21, 21.0}, {0.22, 22.0}};

    Blip c = makeBlip(3, "CHARLIE", 0.3, false);
    c.trail = {{0.30, 30.0}, {0.31, 31.0}, {0.32, 32.0}};

    s.blips.push_back(a);
    s.blips.push_back(b);
    s.blips.push_back(c);

    const std::string j = toJson(s);
    TEST_ASSERT_TRUE(wellFormed(j));
    TEST_ASSERT_TRUE(contains(j, "\"id\":1"));
    TEST_ASSERT_TRUE(contains(j, "\"id\":2"));
    TEST_ASSERT_TRUE(contains(j, "\"id\":3"));
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

// "The core ranks and truncates; renderers never decide what matters" is the
// most explicitly stated constraint on this branch. A truncate-BEFORE-sort
// implementation would pass a same-size-only assertion just as easily as the
// correct truncate-AFTER-sort one, so this pins the actual survivors, not
// just the count.
//
// The 35 hidden blips are pushed FIRST and the 5 visible ones LAST, and every
// blip has a distinct r. That ordering matters: if resize() ran before
// stable_sort(), "the first 12 pushed" would be 12 hidden objects and zero
// visible ones - a completely different (and wrong) set from what ranking
// should produce (all 5 visible, then the 7 lowest-r hidden). A bug that
// truncates first can't hide behind push order here.
void test_rank_caps_at_max_blips(void) {
    Snapshot s;
    for (int i = 0; i < 35; ++i) {
        s.blips.push_back(makeBlip(2000 + i, "HID", 0.51 + 0.01 * i, false));
    }
    for (int i = 0; i < 5; ++i) {
        s.blips.push_back(makeBlip(1000 + i, "VIS", 0.10 + 0.10 * i, true));
    }

    rankAndCap(s);

    TEST_ASSERT_EQUAL_INT(MAX_BLIPS, static_cast<int>(s.blips.size()));

    // All 5 visible objects survive, ranked first, ascending by r.
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_INT(1000 + i, s.blips[i].id);
    }
    // Then the 7 lowest-r hidden objects, also ascending by r.
    for (int i = 0; i < 7; ++i) {
        TEST_ASSERT_EQUAL_INT(2000 + i, s.blips[5 + i].id);
    }
}

void test_rank_leaves_short_lists_alone(void) {
    Snapshot s;
    s.blips.push_back(makeBlip(1, "A", 0.5, true));
    s.blips.push_back(makeBlip(2, "B", 0.6, true));
    rankAndCap(s);
    TEST_ASSERT_EQUAL_INT(2, static_cast<int>(s.blips.size()));
}

void test_json_reports_sun_altitude(void) {
    Snapshot s;
    s.status = ScopeStatus::Ok;
    s.sunAltDeg = -12.75;
    TEST_ASSERT_TRUE(contains(toJson(s), "\"sunAltDeg\":-12.75"));
}

void test_json_reports_blip_reason(void) {
    Snapshot s;
    s.status = ScopeStatus::Ok;
    Blip b = makeBlip(25544, "ISS", 0.5, false);
    b.reason = "eclipsed";
    s.blips.push_back(b);
    TEST_ASSERT_TRUE(contains(toJson(s), "\"reason\":\"eclipsed\""));
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
    RUN_TEST(test_json_emits_elevation_field);
    RUN_TEST(test_json_emits_rings_array);
    RUN_TEST(test_json_empty_snapshot_is_well_formed);
    RUN_TEST(test_json_single_blip_is_well_formed);
    RUN_TEST(test_json_with_multiple_blips_and_trails);
    RUN_TEST(test_rank_puts_visible_objects_first);
    RUN_TEST(test_rank_orders_by_elevation_within_a_group);
    RUN_TEST(test_rank_caps_at_max_blips);
    RUN_TEST(test_rank_leaves_short_lists_alone);
    RUN_TEST(test_json_reports_sun_altitude);
    RUN_TEST(test_json_reports_blip_reason);
    return UNITY_END();
}
