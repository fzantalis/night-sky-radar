#pragma once
#include <cstddef>

// Injected allocator so lib/core/ can place heavyweight payloads (SGP4's
// internal Tle/SGP4 objects, which own several std::string allocations) off
// the internal heap without lib/core/ ever including an ESP-IDF or Arduino
// header. Firmware calls setAllocator() once at boot with PSRAM-backed
// functions; anything that never calls it - native tests included - keeps
// plain malloc/free, so the core stays platform-free and testable.
namespace corealloc {

using AllocFn = void* (*)(std::size_t);
using FreeFn  = void  (*)(void*);

// Installs the active allocator. Passing either argument as nullptr leaves
// that half (alloc or free) unchanged - reset() below is the way to restore
// both to malloc/free.
void setAllocator(AllocFn a, FreeFn f);

// Restores the default malloc/free pair. Mainly useful for tests that
// install a custom allocator and must not leak it into later test cases.
void reset();

// Returns nullptr on failure, exactly like malloc - callers must check.
void* alloc(std::size_t n);
void  free(void* p);

}  // namespace corealloc
