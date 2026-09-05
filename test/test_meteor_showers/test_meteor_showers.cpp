#include <unity.h>
#include <string>

#include "MeteorShowers.h"

namespace {

bool contains(const std::vector<const MeteorShower*>& v, const char* name) {
    for (const MeteorShower* s : v) {
        if (std::string(s->name) == name) return true;
    }
    return false;
}

}  // namespace

// The M4 report date. Early September is genuinely quiet between the
// Perseids (ending Aug 24) and the Orionids (starting Oct 2) - see
// docs/third-party/METEOR-ALMANAC-PROVENANCE.md. Zero is the correct answer
// here, not a bug.
void test_no_shower_active_on_the_m4_report_date(void) {
    const auto v = activeShowers(9, 4);
    TEST_ASSERT_EQUAL(0, static_cast<int>(v.size()));
}

// Each of the eight showers must be active on its own documented peak date.
void test_each_shower_is_active_on_its_own_peak_date(void) {
    struct Case { int month, day; const char* name; };
    const Case cases[] = {
        {1, 3,  "Quadrantids"},
        {4, 22, "April Lyrids"},
        {5, 6,  "Eta Aquariids"},
        {8, 13, "Perseids"},
        {10, 21, "Orionids"},
        {11, 17, "Leonids"},
        {12, 14, "Geminids"},
        {12, 22, "Ursids"},
    };
    for (const Case& c : cases) {
        const auto v = activeShowers(c.month, c.day);
        TEST_ASSERT_TRUE(contains(v, c.name));
    }
}

// --- The year-crossing trap: Quadrantids run Dec 28 - Jan 12. ---------------

void test_quadrantids_active_late_december(void) {
    TEST_ASSERT_TRUE(contains(activeShowers(12, 28), "Quadrantids"));  // window start
    TEST_ASSERT_TRUE(contains(activeShowers(12, 30), "Quadrantids"));
    TEST_ASSERT_TRUE(contains(activeShowers(12, 31), "Quadrantids"));
}

void test_quadrantids_active_early_january(void) {
    TEST_ASSERT_TRUE(contains(activeShowers(1, 1), "Quadrantids"));
    TEST_ASSERT_TRUE(contains(activeShowers(1, 5), "Quadrantids"));
    TEST_ASSERT_TRUE(contains(activeShowers(1, 12), "Quadrantids"));  // window end
}

void test_quadrantids_not_active_outside_the_window(void) {
    TEST_ASSERT_FALSE(contains(activeShowers(12, 27), "Quadrantids"));  // one day early
    TEST_ASSERT_FALSE(contains(activeShowers(1, 13), "Quadrantids"));  // one day late
    TEST_ASSERT_FALSE(contains(activeShowers(6, 15), "Quadrantids"));  // mid-year, nowhere close
}

// --- Ordinary (non-wrapping) window boundaries, spot-checked on Perseids. --

void test_perseids_window_boundaries(void) {
    TEST_ASSERT_TRUE(contains(activeShowers(7, 17), "Perseids"));   // start
    TEST_ASSERT_TRUE(contains(activeShowers(8, 24), "Perseids"));   // end
    TEST_ASSERT_FALSE(contains(activeShowers(7, 16), "Perseids"));  // one day early
    TEST_ASSERT_FALSE(contains(activeShowers(8, 25), "Perseids"));  // one day late
}

// --- Soonest-peaking-first ordering, using a real overlap in the table: ---
// the Orionids (active Oct 2 - Nov 7) and the Leonids (active Nov 6 - Nov 30)
// both cover November 6. The Orionids already peaked (Oct 21); the Leonids
// peak 11 days later (Nov 17). The Leonids must be listed first.

void test_active_showers_ordered_soonest_peak_first(void) {
    const auto v = activeShowers(11, 6);
    TEST_ASSERT_TRUE(contains(v, "Orionids"));
    TEST_ASSERT_TRUE(contains(v, "Leonids"));

    int leonidsIdx = -1, orionidsIdx = -1;
    for (size_t i = 0; i < v.size(); ++i) {
        if (std::string(v[i]->name) == "Leonids")  leonidsIdx = static_cast<int>(i);
        if (std::string(v[i]->name) == "Orionids") orionidsIdx = static_cast<int>(i);
    }
    TEST_ASSERT_TRUE(leonidsIdx >= 0 && orionidsIdx >= 0);
    TEST_ASSERT_TRUE(leonidsIdx < orionidsIdx);
}

// --- Table sanity: every entry has a plausible radiant/ZHR. ---------------

void test_every_shower_has_a_plausible_radiant_and_zhr(void) {
    // Sweep the whole year and check invariants on whatever comes back,
    // rather than reaching into the table directly (there is no accessor for
    // it - activeShowers() is the only public surface).
    for (int month = 1; month <= 12; ++month) {
        const auto v = activeShowers(month, 15);
        for (const MeteorShower* s : v) {
            TEST_ASSERT_TRUE(s->raDeg >= 0.0 && s->raDeg < 360.0);
            TEST_ASSERT_TRUE(s->decDeg >= -90.0 && s->decDeg <= 90.0);
            TEST_ASSERT_TRUE(s->zhr > 0);
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_no_shower_active_on_the_m4_report_date);
    RUN_TEST(test_each_shower_is_active_on_its_own_peak_date);
    RUN_TEST(test_quadrantids_active_late_december);
    RUN_TEST(test_quadrantids_active_early_january);
    RUN_TEST(test_quadrantids_not_active_outside_the_window);
    RUN_TEST(test_perseids_window_boundaries);
    RUN_TEST(test_active_showers_ordered_soonest_peak_first);
    RUN_TEST(test_every_shower_has_a_plausible_radiant_and_zhr);
    return UNITY_END();
}
