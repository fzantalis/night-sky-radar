#pragma once

#include <cstddef>
#include <vector>

#include <Arduino.h>
#include <esp_heap_caps.h>

// Allocates from SPIRAM instead of the internal heap, for the `tracked`
// vector's own backing array. This is deliberately separate from
// CoreAlloc.h: CoreAlloc lives in lib/core/ (no ESP-IDF headers allowed
// there) and covers the per-object SGP4 payloads that a container allocator
// can never reach - relocating a std::vector<Tracked>'s array does not move
// what a std::unique_ptr member inside each Tracked points at. This
// allocator only needs to move the array itself, so it can include
// esp_heap_caps.h directly and stay a small, ordinary Allocator.
//
// Arduino-ESP32 builds with exceptions enabled in this project (see
// platformio.ini), but an allocation failure here still aborts loudly rather
// than returning nullptr - std::vector has no contract for an allocator that
// returns null instead of throwing/aborting, and a silent nullptr would
// corrupt the vector.
template <class T>
struct PsramAllocator {
    using value_type = T;

    PsramAllocator() noexcept = default;
    template <class U>
    constexpr PsramAllocator(const PsramAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        void* p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM);
        if (p == nullptr) {
            Serial.printf("[psram] allocation of %u bytes failed\n",
                          static_cast<unsigned>(n * sizeof(T)));
            abort();
        }
        return static_cast<T*>(p);
    }

    void deallocate(T* p, std::size_t) noexcept {
        heap_caps_free(p);
    }
};

template <class T, class U>
bool operator==(const PsramAllocator<T>&, const PsramAllocator<U>&) { return true; }

template <class T, class U>
bool operator!=(const PsramAllocator<T>&, const PsramAllocator<U>&) { return false; }

template <class T>
using PsramVector = std::vector<T, PsramAllocator<T>>;
