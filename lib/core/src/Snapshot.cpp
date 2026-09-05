#include "Snapshot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Projection.h"

std::vector<Ring> defaultSkyRings() {
    return {
        {60.0, skyRadius(60.0), "grid",    ""},
        {30.0, skyRadius(30.0), "grid",    ""},
        {10.0, skyRadius(10.0), "floor",   ""},
        {0.0,  skyRadius(0.0),  "horizon", ""},
    };
}

std::vector<Ring> defaultNeoRings(double rimLd) {
    if (!(rimLd > 0.0)) return {};

    // Only rings that fall inside the dial are emitted, so shrinking the rim
    // does not produce rings drawn past the edge.
    const double marks[] = {1.0, 2.0, 5.0, 10.0};
    std::vector<Ring> out;
    for (const double ld : marks) {
        if (ld > rimLd) continue;
        Ring r;
        r.elevationDeg = 0.0;              // meaningless here; label carries it
        r.r            = ld / rimLd;
        r.kind         = (ld == 1.0) ? "moon" : "grid";
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%g LD", ld);
        r.label = buf;
        out.push_back(r);
    }
    return out;
}

double estimatedDiameterMetres(double hMag, double albedo) {
    if (!std::isfinite(hMag) || hMag > 90.0) return 0.0;
    if (!std::isfinite(albedo) || albedo <= 0.0) return 0.0;
    // D(km) = 1329 / sqrt(albedo) * 10^(-H/5), then to metres.
    const double km = (1329.0 / std::sqrt(albedo)) * std::pow(10.0, -hMag / 5.0);
    if (!std::isfinite(km) || km < 0.0) return 0.0;
    return km * 1000.0;
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

const char* modeName(ScopeMode m) {
    switch (m) {
        case ScopeMode::Sky: return "sky";
        case ScopeMode::Neo: return "neo";
    }
    return "sky";
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
    j += ",\"mode\":\"";
    j += modeName(s.mode);
    j += "\",\"status\":\"";
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
        j += "\",\"label\":\"";
        j += escape(rg.label);
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
    j += "]";

    j += ",\"radiants\":[";
    for (size_t i = 0; i < s.radiants.size(); ++i) {
        const Radiant& r = s.radiants[i];
        if (i > 0) j += ',';
        j += "{\"name\":\"";
        j += escape(r.name);
        j += "\",\"r\":";
        j += num(r.r, 4);
        j += ",\"theta\":";
        j += num(r.theta, 2);
        j += ",\"el\":";
        j += num(r.elevationDeg, 2);
        j += ",\"zhr\":";
        j += std::to_string(r.zhr);
        j += ",\"atPeak\":";
        j += (r.atPeak ? "true" : "false");
        j += '}';
    }
    j += "]";

    j += ",\"neoRimLd\":";
    j += num(s.neoRimLd, 1);
    j += ",\"neoAgeHours\":";
    j += num(s.neoAgeHours, 2);
    j += ",\"neos\":[";
    for (size_t i = 0; i < s.neos.size(); ++i) {
        const NeoApproachBlip& n = s.neos[i];
        if (i > 0) j += ',';
        j += "{\"name\":\"";
        j += escape(n.name);
        j += "\",\"fullname\":\"";
        j += escape(n.fullname);
        j += "\",\"r\":";
        j += num(n.r, 4);
        j += ",\"theta\":";
        j += num(n.theta, 2);
        j += ",\"distLd\":";
        j += num(n.distLd, 3);
        j += ",\"vRelKmS\":";
        j += num(n.vRelKmS, 2);
        j += ",\"hMag\":";
        j += num(n.hMag, 2);
        j += ",\"hKnown\":";
        j += (n.hKnown ? "true" : "false");
        j += ",\"approachIn\":";
        j += std::to_string(n.approachIn);
        j += ",\"diameterM\":";
        j += num(n.estimatedDiameterM, 1);
        j += '}';
    }
    j += "]}";
    return j;
}
