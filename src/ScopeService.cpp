#include "ScopeService.h"

#include <Arduino.h>
#include <deque>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "Config.h"
#include "Net.h"
#include "PsramAllocator.h"
#include "TleFetcher.h"
#include "TleStore.h"

#include "Projection.h"
#include "Propagator.h"
#include "Solar.h"
#include "StdMagTable.h"
#include "TimeUtils.h"
#include "TleCount.h"
#include "Topocentric.h"
#include "Visibility.h"

namespace {

// ~200 elsetrec-equivalent objects across the two groups. The objects
// themselves (SGP4::Tle/SGP4 payloads, allocated via CoreAlloc in
// Propagator) live in PSRAM regardless of MAX_TRACKED; this cap also bounds
// PsramVector<Tracked>'s own backing array and the per-loop propagation cost.
constexpr int      MAX_TRACKED       = 220;
constexpr uint32_t REFRESH_HOURS     = 12;
constexpr uint32_t BACKOFF_START_MS  = 60UL * 1000UL;      // 1 minute
constexpr uint32_t BACKOFF_MAX_MS    = 60UL * 60UL * 1000UL;  // 60 minutes
constexpr size_t   TRAIL_LEN         = 5;
constexpr uint32_t TRAIL_INTERVAL_MS = 4000;

struct Tracked {
    Tle        tle;
    Propagator prop;
};

// Backing array lives in PSRAM (see PsramAllocator.h). Note this only
// relocates the vector's own array - each Tracked's Propagator allocates its
// SGP4 payload separately, via CoreAlloc, which is what actually keeps the
// ~200-280 KB of per-object state off the internal heap.
PsramVector<Tracked> tracked;
std::map<int, std::deque<std::pair<double, double>>> trails;

uint32_t nextAttemptMs = 0;
uint32_t backoffMs     = BACKOFF_START_MS;
uint32_t lastTrailMs   = 0;

// Per-group validation baselines - the last element count that group
// validated and saved successfully. 0 means "no history yet" (fresh install,
// or that group has never had a successful fetch). Deliberately *not* a
// combined total across both groups: "stations" (~21 objects) and "visual"
// (~150+) have wildly different sizes, and checking a small group against a
// large combined total rejects it forever (see FINDING 1, M2 Task 6 fix
// round 1). Seeded from the cached files' own parse counts in begin(), so a
// reboot does not lose the baseline and immediately reject the next refresh.
int stationsPrevCount = 0;
int visualPrevCount   = 0;

// Rebuilds `tracked` from the two groups' raw text, deduplicating on satnum.
// `visual` and `stations` overlap on the ISS (and possibly others); absorbing
// `stations` first means a shared object keeps stations' element set, and a
// duplicate never draws two blips on top of each other. Used by both the
// cached-file load path (scope::begin()) and a successful fetch
// (attemptRefresh()) - text is parsed exactly once per group either way.
void rebuildFromGroups(const String& stationsRaw, const String& visualRaw) {
    // Verification point (Task 6): PSRAM must fall by ~200 KB here while
    // internal heap stays nearly flat. If internal heap drops instead, the
    // SGP4 payloads are still landing on the wrong heap.
    const uint32_t heapBefore  = ESP.getFreeHeap();
    const uint32_t psramBefore = ESP.getFreePsram();

    tracked.clear();

    std::vector<Tle> parsed;
    std::vector<int> seen;

    auto absorb = [&](const String& raw) {
        std::vector<Tle> batch;
        parseTleText(raw, batch, MAX_TRACKED);
        for (const Tle& t : batch) {
            bool duplicate = false;
            for (const int id : seen) {
                if (id == t.satnum) { duplicate = true; break; }
            }
            if (duplicate) continue;
            if (static_cast<int>(parsed.size()) >= MAX_TRACKED) break;
            seen.push_back(t.satnum);
            parsed.push_back(t);
        }
    };

    absorb(stationsRaw);   // stations first, so the ISS keeps its group's element set
    absorb(visualRaw);

    tracked.reserve(parsed.size());
    for (const Tle& t : parsed) {
        Tracked tr;
        tr.tle = t;
        if (tr.prop.init(t)) {
            // Propagator is move-only; the vector must move Tracked, not copy it.
            tracked.push_back(std::move(tr));
        } else {
            Serial.printf("[scope] propagator init failed for %d\n", t.satnum);
        }
    }

    // Prune trails for satnums that dropped out of the new tracked set (e.g.
    // a decayed object or a catalogue change between refreshes). Without
    // this, trails grows without bound over an unattended device's lifetime,
    // since sampleTrails()/build() only erase entries for objects that are
    // still tracked but currently below the horizon.
    std::set<int> liveSatnums;
    for (const Tracked& tr : tracked) {
        liveSatnums.insert(tr.tle.satnum);
    }
    for (auto it = trails.begin(); it != trails.end(); ) {
        if (liveSatnums.find(it->first) == liveSatnums.end()) {
            it = trails.erase(it);
        } else {
            ++it;
        }
    }

    const uint32_t heapAfter  = ESP.getFreeHeap();
    const uint32_t psramAfter = ESP.getFreePsram();
    Serial.printf("[scope] tracking %u objects, psram free %u\n",
                  static_cast<unsigned>(tracked.size()), psramAfter);
    Serial.printf("[scope] rebuild heap %u -> %u (delta %ld), psram %u -> %u (delta %ld)\n",
                  heapBefore, heapAfter, static_cast<long>(heapAfter) - static_cast<long>(heapBefore),
                  psramBefore, psramAfter, static_cast<long>(psramAfter) - static_cast<long>(psramBefore));
}

bool refreshDue(int64_t nowUnix) {
    const double age = tlestore::ageHours(nowUnix);
    if (age < 0.0) return true;                       // never fetched
    return age >= static_cast<double>(REFRESH_HOURS);
}

void backoffAndRetryLater() {
    backoffMs = (backoffMs * 2 > BACKOFF_MAX_MS) ? BACKOFF_MAX_MS : backoffMs * 2;
    nextAttemptMs = millis() + backoffMs;
}

// Parses `raw` and returns just the element count, for seeding/refreshing a
// group's validation baseline without needing the parsed Tle objects.
int parseGroupCount(const String& raw) {
    std::vector<Tle> tles;
    return parseTleText(raw, tles, MAX_TRACKED);
}

// Validates one group's freshly-fetched body before it's allowed to touch
// flash or replace what's currently tracked. CelesTrak (and a truncated TLS
// read) can return HTTP 200 with a plain-text error body or a partial
// download; fetchGroup() only checks for a non-empty body, so the content
// itself must be validated here. `previousCount` is *this group's own* last
// validated count (see stationsPrevCount/visualPrevCount above) - never a
// combined total across groups. The actual plausibility check lives in
// lib/core's tlecount::tleCountIsPlausible() so it's natively testable;
// this just parses, logs on rejection (naming the group, not "total"), and
// reports the count back to the caller so it can update the baseline on
// success.
bool validateGroupBody(const char* group, const String& raw, int previousCount, int& countOut) {
    std::vector<Tle> tles;
    const int count = parseTleText(raw, tles, MAX_TRACKED);
    countOut = count;

    if (!tlecount::tleCountIsPlausible(count, previousCount)) {
        Serial.printf("[scope] refresh rejected for '%s': parsed %d element set(s) "
                      "(had %d tracked for this group), keeping existing cache\n",
                      group, count, previousCount);
        return false;
    }
    return true;
}

void attemptRefresh(int64_t nowUnix) {
    // Serialised: each fetch fully tears down its TLS client before the next
    // one starts (fetchGroup()'s contract). Two concurrent handshakes would
    // not fit in internal heap.
    String stationsRaw;
    int stationsCount = 0;
    const bool stationsOk = tlefetcher::fetchGroup("stations", stationsRaw)
                          && validateGroupBody("stations", stationsRaw, stationsPrevCount, stationsCount)
                          && tlestore::save("stations", stationsRaw);
    if (stationsOk) stationsPrevCount = stationsCount;

    String visualRaw;
    int visualCount = 0;
    const bool visualOk = tlefetcher::fetchGroup("visual", visualRaw)
                        && validateGroupBody("visual", visualRaw, visualPrevCount, visualCount)
                        && tlestore::save("visual", visualRaw);
    if (visualOk) visualPrevCount = visualCount;

    if (!stationsOk || !visualOk) {
        backoffAndRetryLater();
        Serial.printf("[scope] refresh failed, retrying in %lu s\n",
                      static_cast<unsigned long>(backoffMs / 1000));
        return;
    }

    // Only stamp the age after both cache writes actually succeeded. This
    // goes through TleStore rather than opening the NVS namespace a second
    // time - two read-write Preferences handles on one namespace is a
    // corruption risk.
    tlestore::markFetched(nowUnix);

    backoffMs = BACKOFF_START_MS;
    nextAttemptMs = millis() + 60UL * 1000UL;

    rebuildFromGroups(stationsRaw, visualRaw);
}

void sampleTrails(int64_t nowUnix, const Observer& obs) {
    const double gmst = timeutils::gmstDegrees(timeutils::julianDate(nowUnix));

    // Propagator::positionAt is non-const (the vendored SGP4 mutates through
    // a const-qualified method internally), so this must bind non-const.
    for (Tracked& tr : tracked) {
        Vec3 pos;
        if (!tr.prop.positionAt(nowUnix, pos)) continue;

        const LookAngles la = look(pos, obs, gmst);
        if (la.elDeg < 0.0) {
            trails.erase(tr.tle.satnum);   // below horizon: forget the trail
            continue;
        }

        auto& q = trails[tr.tle.satnum];
        q.emplace_back(skyRadius(la.elDeg), la.azDeg);
        while (q.size() > TRAIL_LEN) q.pop_front();
    }
}

}  // namespace

namespace scope {

void begin() {
    tlestore::begin();

    const String s = tlestore::load("stations");
    const String v = tlestore::load("visual");

    // Seed each group's validation baseline from what's already on flash, so
    // a reboot doesn't forget it and treat the next refresh's real count as
    // unprecedented (previousCount == 0, which would just accept anything -
    // harmless but loses the intended check for one cycle) or, worse, hold
    // onto a stale in-RAM value from before the reboot. An empty/missing
    // cache parses to 0, which is exactly the "no history yet" baseline.
    stationsPrevCount = parseGroupCount(s);
    visualPrevCount   = parseGroupCount(v);

    if (s.length() > 0 || v.length() > 0) {
        Serial.println("[scope] loading cached element sets");
        rebuildFromGroups(s, v);
    }
}

void loop() {
    if (!net::timeValid()) return;

    const int64_t now = net::nowUnix();

    // Rollover-safe: absolute millis() >= nextAttemptMs breaks for ~49 days
    // after millis() wraps, since a pre-wrap nextAttemptMs stays numerically
    // larger than the freshly-wrapped millis() for a long time. Unsigned
    // subtraction wraps correctly, matching the trail timer below.
    if (net::wifiUp() && static_cast<int32_t>(millis() - nextAttemptMs) >= 0 && refreshDue(now)) {
        attemptRefresh(now);
    }

    // hasLocation() takes the NVS mutex for four lookups; only pay for it
    // when the trail timer actually fires, not on every spin of loop().
    if (millis() - lastTrailMs >= TRAIL_INTERVAL_MS) {
        lastTrailMs = millis();
        if (!config::hasLocation()) return;
        sampleTrails(now, config::observer());
    }
}

Snapshot build() {
    Snapshot s;
    s.t = net::nowUnix();

    if (!net::timeValid())      { s.status = ScopeStatus::NoTime;     return s; }
    if (!config::hasLocation()) { s.status = ScopeStatus::NoLocation; return s; }

    s.status      = net::wifiUp() ? ScopeStatus::Ok : ScopeStatus::Offline;
    s.tleAgeHours = tlestore::ageHours(s.t);

    const Observer obs  = config::observer();
    const double   gmst = timeutils::gmstDegrees(timeutils::julianDate(s.t));

    // These depend only on the instant, not on any individual tracked object,
    // so compute them once per snapshot rather than once per blip (up to ~200
    // objects) - recomputing solar position per object is pure waste on a
    // synchronous web server.
    const Vec3   sun       = sunEci(timeutils::julianDate(s.t));
    const Vec3   site      = siteEci(obs, gmst);
    const double sunAltDeg = sunAltitudeDeg(obs, s.t);
    s.sunAltDeg = sunAltDeg;

    // Propagator::positionAt is non-const (see Propagator.h), so this must
    // bind non-const.
    for (Tracked& tr : tracked) {
        Vec3 pos;
        if (!tr.prop.positionAt(s.t, pos)) continue;

        const LookAngles la = look(pos, obs, gmst);
        if (la.elDeg < 0.0) continue;   // below the horizon, not drawable

        Blip b;
        b.id           = tr.tle.satnum;
        b.name         = tr.tle.name;
        b.r            = skyRadius(la.elDeg);
        b.theta        = la.azDeg;
        b.elevationDeg = la.elDeg;

        const Verdict v = judge(la, pos, sun, site, sunAltDeg,
                                 stdMagFor(tr.tle.satnum));
        b.magnitude = v.magnitude;
        b.visible   = v.visible;
        b.reason    = visReasonName(v.reason);

        auto it = trails.find(tr.tle.satnum);
        if (it != trails.end()) {
            b.trail.assign(it->second.begin(), it->second.end());
        }

        s.blips.push_back(b);
    }

    rankAndCap(s);
    return s;
}

void clearTrails() {
    trails.clear();
}

}  // namespace scope
