#include "ScopeService.h"

#include <Arduino.h>
#include <ctime>
#include <deque>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "Config.h"
#include "NeoService.h"
#include "Net.h"
#include "PassTask.h"
#include "PsramAllocator.h"
#include "TleFetcher.h"
#include "TleStore.h"

#include "Celestial.h"
#include "MeteorShowers.h"
#include "Projection.h"
#include "Propagator.h"
#include "Solar.h"
#include "StdMagTable.h"
#include "TimeUtils.h"
#include "TleCount.h"
#include "Topocentric.h"
#include "TrainFilter.h"
#include "Visibility.h"

namespace {

// ~200 elsetrec-equivalent objects across `stations`+`visual`, plus headroom
// for up to MAX_TRAIN more from `starlink` (M3). The objects themselves
// (SGP4::Tle/SGP4 payloads, allocated via CoreAlloc in Propagator) live in
// PSRAM regardless of MAX_TRACKED; this cap also bounds PsramVector<Tracked>'s
// own backing array and the per-loop propagation cost.
constexpr int      MAX_TRACKED       = 300;
constexpr uint32_t REFRESH_HOURS     = 12;
constexpr uint32_t BACKOFF_START_MS  = 60UL * 1000UL;      // 1 minute
constexpr uint32_t BACKOFF_MAX_MS    = 60UL * 60UL * 1000UL;  // 60 minutes
constexpr size_t   TRAIL_LEN         = 5;
constexpr uint32_t TRAIL_INTERVAL_MS = 4000;

// A fresh Starlink batch is ~20-60 objects (plan estimate, and matches what
// CelesTrak actually served on 2026-09-04: one 24-member launch). 80 is a
// safe cap that still bounds fetchGroupStreaming()'s `out` vector.
constexpr int MAX_TRAIN = 80;

struct Tracked {
    Tle        tle;
    Propagator prop;
    bool       isTrain = false;   // "starlink" group membership - see Blip::kind
};

// --- Train filter plumbing --------------------------------------------
//
// fetchGroupStreaming() takes a plain `bool(*)(const Tle&)` function pointer
// (see TleFetcher.h) so it never needs to know about std::function or
// capture storage - but the actual predicate (trainfilter::isTrainCandidate)
// needs a runtime "current year" and a threshold. Stash them here before each
// starlink fetch and read them back from this fixed-signature wrapper, which
// is the only thing actually passed as the TleFilter.
int    g_trainCurrentYear = 0;
double g_trainMinMeanMotion = trainfilter::kDefaultMinMeanMotionRevPerDay;

bool trainFilterFn(const Tle& t) {
    trainfilter::Params p;
    p.minMeanMotionRevPerDay = g_trainMinMeanMotion;
    return trainfilter::isTrainCandidate(t, g_trainCurrentYear, p);
}

// Civil (UTC) year for the mean-motion+year train filter. Deliberately not in
// lib/core: TrainFilter.h takes the year as a plain int precisely so the core
// never needs <ctime> or any notion of "now".
int civilYearUtc(int64_t unixSeconds) {
    const time_t t = static_cast<time_t>(unixSeconds);
    struct tm out;
    gmtime_r(&t, &out);
    return out.tm_year + 1900;
}

// Civil (UTC) month/day for MeteorShowers::activeShowers(), same reasoning as
// civilYearUtc() above: activeShowers() takes plain ints so lib/core never
// needs <ctime>. Using UTC rather than the observer's local date matches the
// precedent already set by the train filter's civilYearUtc() - the observer
// here is fixed in Greece (UTC+2/+3), so this can only be off by at most one
// calendar day right at local midnight, never enough to miss or fabricate an
// entire multi-day activity window.
void civilMonthDayUtc(int64_t unixSeconds, int& month, int& day) {
    const time_t t = static_cast<time_t>(unixSeconds);
    struct tm out;
    gmtime_r(&t, &out);
    month = out.tm_mon + 1;
    day   = out.tm_mday;
}

// Backing array lives in PSRAM (see PsramAllocator.h). Note this only
// relocates the vector's own array - each Tracked's Propagator allocates its
// SGP4 payload separately, via CoreAlloc, which is what actually keeps the
// ~200-280 KB of per-object state off the internal heap.
PsramVector<Tracked> tracked;
std::map<int, std::deque<std::pair<double, double>>> trails;

uint32_t nextAttemptMs = 0;
uint32_t backoffMs     = BACKOFF_START_MS;
uint32_t lastTrailMs   = 0;

// Cached copy of the last build() result, refreshed at most once per second
// from loop(). An HTTP request and the status LED both read this instead of
// triggering their own propagation pass over every tracked object - see
// currentSnapshot() below.
Snapshot lastSnapshot;
uint32_t lastBuildMs = 0;

// Which projection build() produces. Not persisted to NVS on purpose: the
// device should come back in SKY mode after a power cut, because that is the
// mode that answers "is anything up right now".
ScopeMode currentMode = ScopeMode::Sky;

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

// Baseline for the *raw* (pre-filter) starlink parse count - i.e. "does this
// look like the real ~8000-object catalog", never the post-filter train
// count, which is legitimately zero on most days (see attemptRefresh()).
// Unlike stationsPrevCount/visualPrevCount, this is NOT seeded from the
// cached file in begin(): the cache holds only the filtered train members
// (a handful of objects), not the raw catalog, so there is nothing to derive
// a raw-count baseline from after a reboot. It starts at 0 ("no history
// yet") every boot, same as the other two on a fresh install - accepted
// deliberately as a one-cycle gap, matching the reasoning already applied to
// stations/visualPrevCount elsewhere in this file.
int starlinkRawPrevCount = 0;

// Rebuilds `tracked` from the three groups' raw text, deduplicating on
// satnum. `visual` and `stations` overlap on the ISS (and possibly others);
// absorbing `stations` first means a shared object keeps stations' element
// set, and a duplicate never draws two blips on top of each other.
// `starlinkRaw` is absorbed last and only ever holds pre-filtered train
// members (see attemptRefresh()), so nothing here re-applies the mean-motion
// filter - it just marks which satnums came from that group so build() can
// set Blip::kind. Used by both the cached-file load path (scope::begin())
// and a successful fetch (attemptRefresh()) - text is parsed exactly once
// per group either way.
void rebuildFromGroups(const String& stationsRaw, const String& visualRaw,
                       const String& starlinkRaw) {
    // Verification point (Task 6): PSRAM must fall by ~200 KB here while
    // internal heap stays nearly flat. If internal heap drops instead, the
    // SGP4 payloads are still landing on the wrong heap.
    const uint32_t heapBefore  = ESP.getFreeHeap();
    const uint32_t psramBefore = ESP.getFreePsram();

    tracked.clear();

    std::vector<Tle> parsed;
    std::vector<int> seen;
    std::set<int> trainSatnums;

    auto absorb = [&](const String& raw, bool isTrainGroup) {
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
            if (isTrainGroup) trainSatnums.insert(t.satnum);
        }
    };

    absorb(stationsRaw, false);   // stations first, so the ISS keeps its group's element set
    absorb(visualRaw, false);
    absorb(starlinkRaw, true);

    tracked.reserve(parsed.size());
    for (const Tle& t : parsed) {
        Tracked tr;
        tr.tle = t;
        tr.isTrain = trainSatnums.count(t.satnum) > 0;
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

    // Hand the freshly-parsed element sets to the prediction task. It copies
    // them again internally before releasing our mutex, and builds its own
    // Propagator instances - never a reference into `tracked` above, which
    // this same function can clear() from the other core mid-prediction.
    if (config::hasLocation()) {
        passtask::submit(parsed, config::observer());
    }
}

bool refreshDue(int64_t nowUnix) {
    // A fresh timestamp with nothing tracked means the cache is gone while the
    // NVS stamp survived. That is exactly what `pio run -t uploadfs` does: it
    // rewrites the LittleFS partition holding the TLE cache, while the fetch
    // timestamp lives in NVS on a different partition. Without this the device
    // sits blind for up to REFRESH_HOURS, serving zero blips while cheerfully
    // reporting its data is fresh. The backoff in attemptRefresh() still gates
    // retries, so this cannot spin.
    if (tracked.empty()) return true;

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

// Reconstructs three-line CelesTrak text from already-parsed, already-
// filtered Tle entries, so the *filtered* starlink result can go through
// TleStore's ordinary group-cache path (temp-and-rename) exactly like
// `stations`/`visual` - the cache holds only train members, never the raw
// ~8000-object catalog. Tle::name/line1/line2 are retained verbatim from the
// original fetch (see Tle.h), so this is a lossless round-trip.
String serialiseTrainCache(const std::vector<Tle>& trains) {
    String raw;
    for (const Tle& t : trains) {
        raw += t.name;
        raw += '\n';
        raw += t.line1;
        raw += '\n';
        raw += t.line2;
        raw += '\n';
    }
    return raw;
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

    // `starlink` last, and via the streaming path - fetchGroup()'s single-
    // String slurp cannot hold the ~1.7 MB body against ~240 KB of free
    // internal heap. g_trainCurrentYear must be set fresh every cycle (not
    // just once at boot), since a device left running crosses a year
    // boundary eventually.
    g_trainCurrentYear = civilYearUtc(nowUnix);

    std::vector<Tle> trainTles;
    int starlinkTotalParsed = 0;
    uint32_t starlinkMinFreeHeap = 0;
    const int starlinkAccepted = tlefetcher::fetchGroupStreaming(
        "starlink", trainFilterFn, trainTles, MAX_TRAIN,
        &starlinkTotalParsed, &starlinkMinFreeHeap);

    // Plausibility is checked against the RAW catalog size (~8000+), never
    // the filtered/accepted count - that count is legitimately zero on most
    // days (no fresh launch in flight), and tlecount::tleCountIsPlausible()
    // rejects count<=0 unconditionally, which exists to catch a truncated/
    // corrupted stations-or-visual fetch, not to reject "no train right
    // now". Reusing it on the accepted count would make every zero-train day
    // look like a failure and spin the backoff forever.
    const bool starlinkTransportOk = starlinkAccepted >= 0;
    const bool starlinkRawPlausible = starlinkTransportOk &&
        tlecount::tleCountIsPlausible(starlinkTotalParsed, starlinkRawPrevCount);
    String starlinkRaw;
    bool starlinkFreshOk = false;
    if (starlinkRawPlausible) {
        starlinkRaw   = serialiseTrainCache(trainTles);
        starlinkFreshOk = tlestore::save("starlink", starlinkRaw);
    }
    if (starlinkFreshOk) starlinkRawPrevCount = starlinkTotalParsed;

    // A starlink hiccup (a transient CelesTrak block or network drop mid-
    // download - observed live against real hardware: CelesTrak returns
    // HTTP 403 for a GROUP that has not changed since the requester's last
    // successful download of it, since it only updates every 2 hours) must
    // NEVER take stations/visual down with it - "existing stations and
    // visual tracking is unaffected" is a hard M3 done-criterion. If this
    // cycle didn't get a fresh, plausible train list, fall back to whatever
    // train roster is already cached on flash (possibly empty, possibly
    // stale-but-real) rather than feeding rebuildFromGroups() nothing and
    // erasing a train that was already showing.
    if (!starlinkFreshOk) {
        starlinkRaw = tlestore::load("starlink");
    }

    Serial.printf("[scope] starlink streaming fetch: %d triple(s) parsed, %d kept as "
                  "train candidates, min free heap %u bytes, plausible=%d, saved=%d\n",
                  starlinkTotalParsed, starlinkAccepted,
                  static_cast<unsigned>(starlinkMinFreeHeap),
                  static_cast<int>(starlinkRawPlausible), static_cast<int>(starlinkFreshOk));

    // stations/visual are the trust-critical groups this gate has always
    // covered (see FINDING 1 / M2 Task 6) - starlink is deliberately NOT part
    // of it. It still shares their cadence (it was just fetched in the same
    // cycle, after both), but its own success or failure neither blocks nor
    // is blocked by theirs.
    if (!stationsOk || !visualOk) {
        backoffAndRetryLater();
        Serial.printf("[scope] refresh failed, retrying in %lu s\n",
                      static_cast<unsigned long>(backoffMs / 1000));
        return;
    }

    // Only stamp the age after both cache writes actually succeeded. This
    // goes through TleStore rather than opening the NVS namespace a second
    // time - two read-write Preferences handles on one namespace is a
    // corruption risk. Deliberately not gated on starlink - see above.
    tlestore::markFetched(nowUnix);

    backoffMs = BACKOFF_START_MS;
    nextAttemptMs = millis() + 60UL * 1000UL;

    rebuildFromGroups(stationsRaw, visualRaw, starlinkRaw);
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

    const String s  = tlestore::load("stations");
    const String v  = tlestore::load("visual");
    const String st = tlestore::load("starlink");

    // Seed each group's validation baseline from what's already on flash, so
    // a reboot doesn't forget it and treat the next refresh's real count as
    // unprecedented (previousCount == 0, which would just accept anything -
    // harmless but loses the intended check for one cycle) or, worse, hold
    // onto a stale in-RAM value from before the reboot. An empty/missing
    // cache parses to 0, which is exactly the "no history yet" baseline.
    //
    // starlinkRawPrevCount is NOT seeded here - the cached starlink file
    // holds only filtered train members, not the raw catalog size that
    // baseline is meant to track (see its declaration above). It starts at 0
    // every boot.
    stationsPrevCount = parseGroupCount(s);
    visualPrevCount   = parseGroupCount(v);

    if (s.length() > 0 || v.length() > 0 || st.length() > 0) {
        Serial.println("[scope] loading cached element sets");
        rebuildFromGroups(s, v, st);
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

    // Serialised with the TLE refresh above by virtue of both running on this
    // one loop - never two TLS handshakes at once, the same rule the CelesTrak
    // group fetches follow among themselves.
    neoservice::loop(now);

    // Rebuild the cached snapshot at most once per second. Rollover-safe, same
    // pattern as the trail timer below. This runs ahead of the hasLocation()
    // early-return just below so the cache still reflects NoLocation status
    // (build() itself checks) rather than getting stuck on whatever the last
    // build happened to be - the status LED and every HTTP request read only
    // this cached copy now, instead of each triggering its own propagation
    // pass over ~200 tracked objects.
    if (millis() - lastBuildMs >= 1000) {
        lastBuildMs = millis();
        lastSnapshot = build();
    }

    // hasLocation() takes the NVS mutex for four lookups; it is called both by
    // build() (~1 Hz via the snapshot cache) and when the trail timer fires.
    if (millis() - lastTrailMs >= TRAIL_INTERVAL_MS) {
        lastTrailMs = millis();
        if (!config::hasLocation()) return;
        sampleTrails(now, config::observer());
    }
}

const Snapshot& currentSnapshot() {
    return lastSnapshot;
}

// Fills in the NEO projection. Takes the snapshot by value with `t`, `mode`
// and `status` already decided, so the two build paths cannot disagree about
// the instant they are describing.
static Snapshot buildNeo(Snapshot s) {
    s.status      = net::wifiUp() ? ScopeStatus::Ok : ScopeStatus::Offline;
    s.neoRimLd    = neoservice::NEO_RIM_LD;
    s.neoAgeHours = neoservice::ageHours(s.t);
    s.rings       = defaultNeoRings(neoservice::NEO_RIM_LD);

    for (const neo::Approach& a : neoservice::approaches()) {
        const int64_t in = a.approachUnix - s.t;

        // Drop approaches that have already happened. The fetch asks for
        // date-min=now, but the cache lives up to NEO_REFRESH_HOURS, so by the
        // end of that window the first few entries can be in the past. Left
        // in, they would wrap round to just under 360 degrees and read as
        // nearly a month away - the exact opposite of the truth.
        if (in < 0) continue;

        NeoApproachBlip n;
        n.name     = a.des;
        n.fullname = a.fullname;
        neo::project(a, s.t, static_cast<double>(neoservice::NEO_WINDOW_DAYS),
                     neoservice::NEO_RIM_LD, n.r, n.theta);
        n.distLd     = a.distLd;
        n.vRelKmS    = a.vRelKmS;
        n.hMag       = a.hMag;
        n.hKnown     = a.hKnown;
        n.approachIn = in;
        n.estimatedDiameterM = a.hKnown ? estimatedDiameterMetres(a.hMag) : 0.0;
        s.neos.push_back(n);
    }
    return s;
}

Snapshot build() {
    Snapshot s;
    s.t    = net::nowUnix();
    s.mode = currentMode;

    if (!net::timeValid())      { s.status = ScopeStatus::NoTime;     return s; }

    // Before the location check, deliberately: an asteroid close approach is
    // an Earth-centred event, so NEO mode works on a device that has never
    // been told where it is.
    if (currentMode == ScopeMode::Neo) return buildNeo(s);

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
        b.kind         = tr.isTrain ? "train" : "sat";
        b.launchYear   = tr.tle.launchYear;
        b.launchNumber = tr.tle.launchNumber;

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

    s.events = passtask::upcoming(s.t);

    // M4: meteor shower radiants. Fixed points on the celestial sphere, no
    // propagation - only shown when the shower is active AND the radiant is
    // above the horizon AND the sky is dark enough (meteors are invisible in
    // daylight, matching the same MAX_SUN_ALT_DEG gate Visibility.h applies
    // to satellite blips).
    if (sunAltDeg < MAX_SUN_ALT_DEG) {
        int month = 0, day = 0;
        civilMonthDayUtc(s.t, month, day);
        for (const MeteorShower* sh : activeShowers(month, day)) {
            const LookAngles la = radiantLookAngles(sh->raDeg, sh->decDeg, obs, s.t);
            if (la.elDeg < 0.0) continue;   // below the horizon, not observable yet

            Radiant r;
            r.name         = sh->name;
            r.r            = skyRadius(la.elDeg);
            r.theta        = la.azDeg;
            r.elevationDeg = la.elDeg;
            r.zhr          = sh->zhr;
            r.atPeak       = (month == sh->peakMonth && day == sh->peakDay);
            s.radiants.push_back(r);
        }
    }

    rankAndCap(s);
    return s;
}

ScopeMode mode() { return currentMode; }

void setMode(ScopeMode m) {
    if (m == currentMode) return;
    currentMode = m;
    // Rebuild immediately rather than letting the once-per-second cache serve
    // a snapshot in the old mode - a mode switch that takes a visible moment
    // to appear reads as a missed keypress.
    lastBuildMs  = millis();
    lastSnapshot = build();
}

void toggleMode() {
    setMode(currentMode == ScopeMode::Sky ? ScopeMode::Neo : ScopeMode::Sky);
}

void clearTrails() {
    trails.clear();
}

}  // namespace scope
