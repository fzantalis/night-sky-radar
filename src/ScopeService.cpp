#include "ScopeService.h"

#include <Arduino.h>
#include <deque>
#include <map>
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

void rebuildFrom(const String& raw) {
    std::vector<Tle> tles;
    const int count = parseTleText(raw, tles, MAX_TRACKED);

    tracked.clear();
    tracked.reserve(static_cast<size_t>(count));

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
}

bool refreshDue(int64_t nowUnix) {
    const double age = tlestore::ageHours(nowUnix);
    if (age < 0.0) return true;                       // never fetched
    return age >= static_cast<double>(REFRESH_HOURS);
}

void attemptRefresh(int64_t nowUnix) {
    String raw;
    if (!tlefetcher::fetchGroup("stations", raw)) {
        backoffMs = (backoffMs * 2 > BACKOFF_MAX_MS) ? BACKOFF_MAX_MS : backoffMs * 2;
        nextAttemptMs = millis() + backoffMs;
        Serial.printf("[scope] refresh failed, retrying in %lu s\n",
                      static_cast<unsigned long>(backoffMs / 1000));
        return;
    }

    if (!tlestore::save(raw)) {
        backoffMs = (backoffMs * 2 > BACKOFF_MAX_MS) ? BACKOFF_MAX_MS : backoffMs * 2;
        nextAttemptMs = millis() + backoffMs;
        return;
    }

    // Only stamp the age after the cache write actually succeeded. This goes
    // through TleStore rather than opening the NVS namespace a second time -
    // two read-write Preferences handles on one namespace is a corruption risk.
    tlestore::markFetched(nowUnix);

    backoffMs = BACKOFF_START_MS;
    nextAttemptMs = millis() + 60UL * 1000UL;

    rebuildFrom(raw);
}

void sampleTrails(int64_t nowUnix, const Observer& obs) {
    const double gmst = timeutils::gmstDegrees(timeutils::julianDate(nowUnix));

    for (const Tracked& tr : tracked) {
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

    if (net::wifiUp() && millis() >= nextAttemptMs && refreshDue(now)) {
        attemptRefresh(now);
    }

    if (!config::hasLocation()) return;

    if (millis() - lastTrailMs >= TRAIL_INTERVAL_MS) {
        lastTrailMs = millis();
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

    for (const Tracked& tr : tracked) {
        Vec3 pos;
        if (!tr.prop.positionAt(s.t, pos)) continue;

        const LookAngles la = look(pos, obs, gmst);
        if (la.elDeg < 0.0) continue;   // below the horizon, not drawable

        Blip b;
        b.id        = tr.tle.satnum;
        b.name      = tr.tle.name;
        b.r         = skyRadius(la.elDeg);
        b.theta     = la.azDeg;
        b.magnitude = 99.0;    // M2 computes this
        b.visible   = false;   // M2 computes this

        auto it = trails.find(tr.tle.satnum);
        if (it != trails.end()) {
            b.trail.assign(it->second.begin(), it->second.end());
        }

        s.blips.push_back(b);
    }

    rankAndCap(s);
    return s;
}

}  // namespace scope
