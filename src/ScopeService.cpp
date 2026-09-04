#include "ScopeService.h"

#include <Arduino.h>
#include <deque>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "Config.h"
#include "Net.h"
#include "TleFetcher.h"
#include "TleStore.h"

#include "Projection.h"
#include "Propagator.h"
#include "TimeUtils.h"
#include "Topocentric.h"

namespace {

constexpr int      MAX_TRACKED       = 40;
constexpr uint32_t REFRESH_HOURS     = 12;
constexpr uint32_t BACKOFF_START_MS  = 60UL * 1000UL;      // 1 minute
constexpr uint32_t BACKOFF_MAX_MS    = 60UL * 60UL * 1000UL;  // 60 minutes
constexpr size_t   TRAIL_LEN         = 5;
constexpr uint32_t TRAIL_INTERVAL_MS = 4000;

struct Tracked {
    Tle        tle;
    Propagator prop;
};

std::vector<Tracked> tracked;
std::map<int, std::deque<std::pair<double, double>>> trails;

uint32_t nextAttemptMs = 0;
uint32_t backoffMs     = BACKOFF_START_MS;
uint32_t lastTrailMs   = 0;

// Rebuilds `tracked` from an already-parsed element set. Callers that only
// have raw text (the cached-file load path) go through rebuildFrom() below,
// which parses once and delegates here; attemptRefresh() parses once for
// validation and passes the same vector straight in, so the fetched body is
// never parsed twice.
void rebuildFromParsed(std::vector<Tle>& tles) {
    tracked.clear();
    tracked.reserve(tles.size());

    for (const Tle& t : tles) {
        Tracked tr;
        tr.tle = t;
        if (tr.prop.init(t)) {
            // Propagator is move-only; the vector must move Tracked, not copy it.
            tracked.push_back(std::move(tr));
        } else {
            Serial.printf("[scope] propagator init failed for %d\n", t.satnum);
        }
    }

    Serial.printf("[scope] tracking %u objects\n",
                  static_cast<unsigned>(tracked.size()));

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
}

// Cached-file load path: the only caller that starts from raw text rather
// than an already-parsed vector.
void rebuildFrom(const String& raw) {
    std::vector<Tle> tles;
    parseTleText(raw, tles, MAX_TRACKED);
    rebuildFromParsed(tles);
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

void attemptRefresh(int64_t nowUnix) {
    String raw;
    if (!tlefetcher::fetchGroup("stations", raw)) {
        backoffAndRetryLater();
        Serial.printf("[scope] refresh failed, retrying in %lu s\n",
                      static_cast<unsigned long>(backoffMs / 1000));
        return;
    }

    // Parse and validate BEFORE anything touches flash or the freshness
    // stamp. CelesTrak (and a truncated TLS read) can return HTTP 200 with a
    // plain-text error body or a partial download; fetchGroup() only checks
    // for a non-empty body, so the content itself must be validated here. A
    // bad body must never overwrite a good cache, never get stamped fresh
    // (that would block retry for REFRESH_HOURS), and never replace `tracked`.
    std::vector<Tle> tles;
    const int count = parseTleText(raw, tles, MAX_TRACKED);
    const int previouslyTracked = static_cast<int>(tracked.size());

    // Reject an empty parse outright, and reject a parse that yields fewer
    // than half of what we were already tracking - that catches a partial
    // download that still happens to parse cleanly (e.g. a truncated TLS
    // read cut mid-file at a triple boundary).
    const bool empty   = (count == 0);
    const bool partial = (previouslyTracked > 0) && (count * 2 < previouslyTracked);
    if (empty || partial) {
        Serial.printf("[scope] refresh rejected: parsed %d element set(s) "
                      "(had %d tracked), keeping existing cache\n",
                      count, previouslyTracked);
        backoffAndRetryLater();
        return;
    }

    if (!tlestore::save(raw)) {
        backoffAndRetryLater();
        return;
    }

    // Only stamp the age after the cache write actually succeeded. This goes
    // through TleStore rather than opening the NVS namespace a second time -
    // two read-write Preferences handles on one namespace is a corruption risk.
    tlestore::markFetched(nowUnix);

    backoffMs = BACKOFF_START_MS;
    nextAttemptMs = millis() + 60UL * 1000UL;

    // Drive the rebuild from the vector we already parsed above - never parse
    // the fetched body twice.
    rebuildFromParsed(tles);
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

    const String cached = tlestore::load();
    if (cached.length() > 0) {
        Serial.println("[scope] loading cached element sets");
        rebuildFrom(cached);
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
        b.magnitude    = 99.0;    // M2 computes this
        b.visible      = false;   // M2 computes this

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
