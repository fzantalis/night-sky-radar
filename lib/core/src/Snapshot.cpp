#include "Snapshot.h"

#include <algorithm>
#include <cstdio>

#include "Projection.h"

std::vector<Ring> defaultSkyRings() {
    return {
        {60.0, skyRadius(60.0), "grid"},
        {30.0, skyRadius(30.0), "grid"},
        {10.0, skyRadius(10.0), "floor"},
        {0.0,  skyRadius(0.0),  "horizon"},
    };
}

namespace {

const char* statusName(ScopeStatus s) {
    switch (s) {
        case ScopeStatus::Ok:          return "ok";
        case ScopeStatus::NoTime:      return "no_time";
        case ScopeStatus::NoLocation:  return "no_location";
        case ScopeStatus::Offline:     return "offline";
    }
    return "no_time";
}

// Minimal JSON string escaping. Object names come from CelesTrak and are plain
// ASCII, but a quote or backslash must never be able to break the document.
std::string escape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (const char c : in) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) break;  // drop control chars
                out += c;
        }
    }
    return out;
}

std::string num(double v, int decimals) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return std::string(buf);
}

}  // namespace

void rankAndCap(Snapshot& s) {
    std::stable_sort(s.blips.begin(), s.blips.end(),
        [](const Blip& a, const Blip& b) {
            if (a.visible != b.visible) return a.visible;  // visible first
            return a.r < b.r;                              // then higher in sky
        });

    if (static_cast<int>(s.blips.size()) > MAX_BLIPS) {
        s.blips.resize(static_cast<size_t>(MAX_BLIPS));
    }
}

std::string toJson(const Snapshot& s) {
    std::string j;
    j.reserve(256 + s.blips.size() * 160);

    j += "{\"t\":";
    j += std::to_string(s.t);
    j += ",\"status\":\"";
    j += statusName(s.status);
    j += "\",\"tleAgeHours\":";
    j += num(s.tleAgeHours, 2);
    j += ",\"sunAltDeg\":";
    j += num(s.sunAltDeg, 2);
    j += ",\"blips\":[";

    for (size_t i = 0; i < s.blips.size(); ++i) {
        const Blip& b = s.blips[i];
        if (i > 0) j += ',';
        j += "{\"id\":";
        j += std::to_string(b.id);
        j += ",\"name\":\"";
        j += escape(b.name);
        j += "\",\"r\":";
        j += num(b.r, 4);
        j += ",\"theta\":";
        j += num(b.theta, 2);
        j += ",\"el\":";
        j += num(b.elevationDeg, 2);
        j += ",\"mag\":";
        j += num(b.magnitude, 2);
        j += ",\"visible\":";
        j += (b.visible ? "true" : "false");
        j += ",\"reason\":\"";
        j += escape(b.reason);
        j += '"';
        j += ",\"kind\":\"";
        j += escape(b.kind);
        j += '"';
        j += ",\"launchYear\":";
        j += std::to_string(b.launchYear);
        j += ",\"launchNumber\":";
        j += std::to_string(b.launchNumber);
        j += ",\"trail\":[";
        for (size_t k = 0; k < b.trail.size(); ++k) {
            if (k > 0) j += ',';
            j += '[';
            j += num(b.trail[k].first, 4);
            j += ',';
            j += num(b.trail[k].second, 2);
            j += ']';
        }
        j += "]}";
    }

    j += "]";

    j += ",\"rings\":[";
    for (size_t i = 0; i < s.rings.size(); ++i) {
        const Ring& rg = s.rings[i];
        if (i > 0) j += ',';
        j += "{\"el\":";
        j += num(rg.elevationDeg, 0);
        j += ",\"r\":";
        j += num(rg.r, 4);
        j += ",\"kind\":\"";
        j += escape(rg.kind);
        j += "\"}";
    }
    j += "]";

    j += ",\"events\":[";
    for (size_t i = 0; i < s.events.size(); ++i) {
        const Event& e = s.events[i];
        if (i > 0) j += ',';
        j += "{\"name\":\"";
        j += escape(e.name);
        j += "\",\"startsIn\":";
        j += std::to_string(e.startsIn);
        j += ",\"maxEl\":";
        j += num(e.maxEl, 1);
        j += ",\"visible\":";
        j += (e.visible ? "true" : "false");
        j += '}';
    }
    j += "]}";
    return j;
}
