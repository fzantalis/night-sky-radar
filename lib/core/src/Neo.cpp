#include "Neo.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace neo {

namespace {

const char* skipWs(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    return p;
}

// Reads a JSON string beginning at its opening quote. Returns the position
// just past the closing quote, or nullptr if the string is unterminated.
const char* readString(const char* p, std::string& out) {
    out.clear();
    if (*p != '"') return nullptr;
    ++p;
    while (*p != '\0') {
        if (*p == '\\') {
            ++p;
            if (*p == '\0') return nullptr;
            switch (*p) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                // Anything else escaped stands for itself, which covers the
                // backslash-quote and backslash-slash this API actually emits.
                // A \uXXXX escape would come through as a literal 'u' -
                // acceptable, because designations and object names in this
                // catalogue are ASCII.
                default:  out.push_back(*p); break;
            }
            ++p;
            continue;
        }
        if (*p == '"') return p + 1;
        out.push_back(*p);
        ++p;
    }
    return nullptr;  // unterminated
}

// Finds a key and returns the position of its value.
//
// This is a substring search, not a structural walk: it would be fooled by a
// key name appearing inside a string value. That is acceptable here and only
// here, because the CAD schema is fixed and none of its values (a designation,
// a date, a catalogue name) can contain the key names this file looks for.
const char* findValue(const char* json, const char* key) {
    const std::string quoted = std::string("\"") + key + "\"";
    const char* at = std::strstr(json, quoted.c_str());
    if (at == nullptr) return nullptr;
    at = skipWs(at + quoted.size());
    if (*at != ':') return nullptr;
    return skipWs(at + 1);
}

// Reads a bracketed array of JSON strings. A null element becomes an empty
// entry, which keeps column positions aligned - dropping it would silently
// shift every later column in the row.
const char* readStringArray(const char* p, std::vector<std::string>& out) {
    p = skipWs(p);
    if (*p != '[') return nullptr;
    ++p;
    while (true) {
        p = skipWs(p);
        if (*p == '\0') return nullptr;
        if (*p == ']') return p + 1;
        if (*p == ',') { ++p; continue; }
        if (std::strncmp(p, "null", 4) == 0) {
            out.emplace_back();      // absent value, e.g. an unknown magnitude
            p += 4;
            continue;
        }
        if (*p != '"') return nullptr;
        std::string s;
        p = readString(p, s);
        if (p == nullptr) return nullptr;
        out.push_back(s);
    }
}

int columnIndex(const std::vector<std::string>& fields, const char* name) {
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (fields[i] == name) return static_cast<int>(i);
    }
    return -1;
}

// Column lookup that tolerates a short row or a null cell.
const std::string* cell(const std::vector<std::string>& row, int index) {
    if (index < 0 || index >= static_cast<int>(row.size())) return nullptr;
    if (row[index].empty()) return nullptr;
    return &row[index];
}

}  // namespace

int64_t julianDateToUnix(double jd) {
    // 2440587.5 is the Julian date of 1970-01-01T00:00:00Z.
    return static_cast<int64_t>(std::llround((jd - 2440587.5) * 86400.0));
}

bool parseCad(const char* json, std::vector<Approach>& out, int maxOut) {
    out.clear();
    if (json == nullptr) return false;

    // Authenticate the payload before trusting anything in it. A CAD response
    // always carries `count`; an error page, a captive portal, or the API's own
    // error body of {"code","message"} does not. Same parse-before-commit rule
    // the CelesTrak fetch follows.
    const char* countAt = findValue(json, "count");
    if (countAt == nullptr) return false;

    long count = 0;
    if (*countAt == '"') {
        std::string s;
        if (readString(countAt, s) == nullptr) return false;
        count = std::strtol(s.c_str(), nullptr, 10);
    } else {
        count = std::strtol(countAt, nullptr, 10);
    }

    // A genuine "nothing is coming close" response omits `fields` and `data`
    // entirely - it is literally {"count":0,"signature":{...}}. That is a
    // successful empty result, not a malformed one, and must not be reported
    // as a failure or it becomes indistinguishable from a network fault.
    if (count <= 0) return true;

    const char* fieldsAt = findValue(json, "fields");
    if (fieldsAt == nullptr) return false;
    std::vector<std::string> fields;
    if (readStringArray(fieldsAt, fields) == nullptr) return false;

    const int iDes  = columnIndex(fields, "des");
    const int iJd   = columnIndex(fields, "jd");
    const int iDist = columnIndex(fields, "dist");
    const int iVrel = columnIndex(fields, "v_rel");
    const int iH    = columnIndex(fields, "h");
    const int iFull = columnIndex(fields, "fullname");

    // Designation, time and distance are the three the dial cannot be drawn
    // without. Velocity, magnitude and full name are enrichment.
    if (iDes < 0 || iJd < 0 || iDist < 0) return false;

    const char* p = findValue(json, "data");
    if (p == nullptr) return false;
    p = skipWs(p);
    if (*p != '[') return false;
    ++p;

    while (true) {
        p = skipWs(p);
        if (*p == '\0') return false;      // truncated body
        if (*p == ']') break;
        if (*p == ',') { ++p; continue; }

        std::vector<std::string> row;
        p = readStringArray(p, row);
        if (p == nullptr) return false;

        const std::string* des  = cell(row, iDes);
        const std::string* jd   = cell(row, iJd);
        const std::string* dist = cell(row, iDist);
        if (des == nullptr || jd == nullptr || dist == nullptr) continue;

        if (maxOut > 0 && static_cast<int>(out.size()) >= maxOut) break;

        Approach a;
        a.des          = *des;
        a.jd           = std::strtod(jd->c_str(), nullptr);
        a.approachUnix = julianDateToUnix(a.jd);
        a.distAu       = std::strtod(dist->c_str(), nullptr);
        a.distLd       = a.distAu / AU_PER_LD;

        if (const std::string* v = cell(row, iVrel)) {
            a.vRelKmS = std::strtod(v->c_str(), nullptr);
        }
        if (const std::string* h = cell(row, iH)) {
            a.hMag   = std::strtod(h->c_str(), nullptr);
            a.hKnown = true;
        }
        if (const std::string* f = cell(row, iFull)) {
            // The API pads fullname with leading spaces for column alignment.
            const std::size_t b = f->find_first_not_of(' ');
            const std::size_t e = f->find_last_not_of(' ');
            if (b != std::string::npos) a.fullname = f->substr(b, e - b + 1);
        }

        out.push_back(a);
    }

    return true;
}

void project(const Approach& a, int64_t nowUnix, double windowDays,
             double rimLd, double& r, double& theta) {
    r     = 0.0;
    theta = 0.0;

    if (rimLd > 0.0) {
        r = a.distLd / rimLd;
        if (r < 0.0) r = 0.0;
        if (r > 1.0) r = 1.0;
    }

    if (windowDays > 0.0) {
        const double windowSec = windowDays * 86400.0;
        const double dt = static_cast<double>(a.approachUnix - nowUnix);
        theta = 360.0 * dt / windowSec;
        theta = std::fmod(theta, 360.0);
        if (theta < 0.0) theta += 360.0;
    }
}

}  // namespace neo
