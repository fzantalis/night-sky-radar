#include "StdMagTable.h"

#include <cstddef>

namespace {

struct Entry {
    int    norad;
    double stdMag;
};

// Seed table of widely published values for objects that dominate the visual
// group. Extend from the McCants/Molczan file if licensing permitted; the
// default covers everything absent here.
constexpr Entry TABLE[] = {
    { 25544, -1.8 },   // ISS (ZARYA)
    { 48274, -1.0 },   // CSS (TIANHE) - Chinese space station core
    { 20580,  2.0 },   // Hubble Space Telescope
    { 27386,  3.5 },   // Envisat
    { 25861,  3.0 },   // Okean O
    { 23405,  2.5 },   // SL-16 rocket body
};

}  // namespace

double stdMagFor(int noradId) {
    for (std::size_t i = 0; i < sizeof(TABLE) / sizeof(TABLE[0]); ++i) {
        if (TABLE[i].norad == noradId) return TABLE[i].stdMag;
    }
    return DEFAULT_STD_MAG;
}
