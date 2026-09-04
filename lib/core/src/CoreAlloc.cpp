#include "CoreAlloc.h"

#include <cstdlib>

namespace corealloc {

namespace {
AllocFn g_alloc = &std::malloc;
FreeFn  g_free  = &std::free;
}  // namespace

void setAllocator(AllocFn a, FreeFn f) {
    if (a != nullptr) g_alloc = a;
    if (f != nullptr) g_free  = f;
}

void reset() {
    g_alloc = &std::malloc;
    g_free  = &std::free;
}

void* alloc(std::size_t n) {
    return g_alloc(n);
}

void free(void* p) {
    g_free(p);
}

}  // namespace corealloc
