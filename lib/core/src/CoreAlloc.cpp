#include "CoreAlloc.h"

#include <cassert>
#include <cstdlib>

namespace corealloc {

namespace {
AllocFn g_alloc     = &std::malloc;
FreeFn  g_free      = &std::free;
bool    g_installed = false;
}  // namespace

// Call-exactly-once contract: firmware calls this once, in main.cpp's
// setup(), before anything in lib/core can allocate. Nothing tags which
// allocator/free pair a given block came from, so installing a second pair
// without an intervening reset() would let a block allocated under the first
// pair get freed under the second - silent heap corruption, and nothing
// would catch it at the point of the bad free. assert() catches a violation
// of that contract in debug builds; reset() (below) is the sanctioned way to
// re-arm this for a test that installs its own pair and must not leak it
// into later test cases.
void setAllocator(AllocFn a, FreeFn f) {
    assert(!g_installed && "corealloc::setAllocator called twice without reset() - see call-once contract above");
    if (a != nullptr) g_alloc = a;
    if (f != nullptr) g_free  = f;
    g_installed = true;
}

void reset() {
    g_alloc     = &std::malloc;
    g_free      = &std::free;
    g_installed = false;
}

void* alloc(std::size_t n) {
    return g_alloc(n);
}

void free(void* p) {
    g_free(p);
}

}  // namespace corealloc
