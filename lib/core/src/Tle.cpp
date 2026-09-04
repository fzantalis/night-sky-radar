#include "Tle.h"

#include <cstdlib>
#include <cstring>
#include <cctype>

namespace {

constexpr int TLE_LINE_LEN = 69;

// Copies a fixed-width column range (1-indexed, inclusive) into a scratch
// buffer and converts it to a double. TLE fields are space-padded, and strtod
// tolerates the leading spaces.
double fieldAsDouble(const char* line, int firstCol, int lastCol) {
    char buf[32] = {0};
    const int n = lastCol - firstCol + 1;
    std::memcpy(buf, line + (firstCol - 1), static_cast<size_t>(n));
    buf[n] = '\0';
    return std::strtod(buf, nullptr);
}

int fieldAsInt(const char* line, int firstCol, int lastCol) {
    char buf[32] = {0};
    const int n = lastCol - firstCol + 1;
    std::memcpy(buf, line + (firstCol - 1), static_cast<size_t>(n));
    buf[n] = '\0';
    return static_cast<int>(std::strtol(buf, nullptr, 10));
}

}  // namespace

bool tleChecksumValid(const char* line) {
    if (line == nullptr) return false;
    if (std::strlen(line) < static_cast<size_t>(TLE_LINE_LEN)) return false;

    int sum = 0;
    for (int i = 0; i < TLE_LINE_LEN - 1; ++i) {
        const char c = line[i];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            sum += c - '0';
        } else if (c == '-') {
            sum += 1;
        }
    }

    const char expected = line[TLE_LINE_LEN - 1];
    if (!std::isdigit(static_cast<unsigned char>(expected))) return false;

    return (sum % 10) == (expected - '0');
}

bool parseTle(const char* name, const char* l1, const char* l2, Tle& out) {
    if (name == nullptr || l1 == nullptr || l2 == nullptr) return false;

    if (std::strlen(l1) < static_cast<size_t>(TLE_LINE_LEN)) return false;
    if (std::strlen(l2) < static_cast<size_t>(TLE_LINE_LEN)) return false;

    if (l1[0] != '1' || l2[0] != '2') return false;
    if (!tleChecksumValid(l1) || !tleChecksumValid(l2)) return false;

    const int satnum1 = fieldAsInt(l1, 3, 7);
    const int satnum2 = fieldAsInt(l2, 3, 7);
    if (satnum1 == 0 || satnum1 != satnum2) return false;

    Tle t;
    t.satnum = satnum1;

    // Columns 10-11 hold a two-digit launch year. The Space-Track convention is
    // that 57-99 mean 1957-1999 and 00-56 mean 2000-2056.
    const int yy = fieldAsInt(l1, 10, 11);
    t.launchYear   = (yy < 57) ? (2000 + yy) : (1900 + yy);
    t.launchNumber = fieldAsInt(l1, 12, 14);

    t.inclinationDeg      = fieldAsDouble(l2, 9, 16);
    t.meanMotionRevPerDay = fieldAsDouble(l2, 53, 63);

    std::strncpy(t.name, name, sizeof(t.name) - 1);
    std::strncpy(t.line1, l1, sizeof(t.line1) - 1);
    std::strncpy(t.line2, l2, sizeof(t.line2) - 1);

    out = t;
    return true;
}
