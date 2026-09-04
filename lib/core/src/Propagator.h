#pragma once

#include <cstdint>
#include <memory>

#include "Vec3.h"
#include "Tle.h"
#include "CoreAlloc.h"

// Forward-declared so the vendored SGP4 headers do not leak into every
// translation unit that merely wants to propagate an orbit.
namespace libsgp4 { class SGP4; class Tle; }

// Wraps a vendored SGP4 implementation (dnwrnr/sgp4, Apache-2.0).
//
// Output is TEME (True Equator, Mean Equinox) in kilometres. This is *not*
// J2000 ECI, and that is deliberate: TEME paired with GMST is the correct
// combination for converting to an Earth-fixed frame, which is exactly what
// Topocentric does. Do not "fix" this by adding precession.
//
// The library signals errors by throwing; this class catches everything and
// converts it to a false return. No exception escapes.
//
// NOT THREAD-SAFE. The vendored libsgp4::SGP4::FindPosition() is declared
// const while holding a mutable IntegratorParams struct that it writes
// through on every call - it mutates state behind a const-qualified API.
// positionAt() is therefore deliberately NOT const here either: a const
// Propagator& is exactly what looks safe to hand to another task, and it is
// not. One Propagator instance per task; never share one across tasks, and
// never call positionAt() on the same instance concurrently from two tasks.
class Propagator {
public:
    Propagator();
    ~Propagator();
    Propagator(const Propagator&) = delete;
    Propagator& operator=(const Propagator&) = delete;
    Propagator(Propagator&&) noexcept;
    Propagator& operator=(Propagator&&) noexcept;

    bool init(const Tle& tle);

    // Returns false if propagation fails. `posKm` is left untouched then.
    // Not const - see the thread-safety note on the class above.
    bool positionAt(int64_t unixSeconds, Vec3& posKm);

    double epochJd() const { return epochJd_; }
    bool   ready()   const { return ready_; }

private:
    // tle_/sgp4_ are allocated via corealloc::alloc() + placement new and
    // released via an explicit destructor call + corealloc::free(), not
    // `delete` - that's how the payload (and the std::string allocations
    // inside libsgp4::Tle) end up in PSRAM on the firmware build instead of
    // the internal heap. Complete types for libsgp4::Tle/SGP4 aren't visible
    // in this header (forward-declared only, see the class comment above),
    // so these deleters are declared here but their operator()s are defined
    // in Propagator.cpp, where the vendored headers are included - the same
    // reason ~Propagator() is `= default` out-of-line rather than inline.
    struct TleDeleter  { void operator()(libsgp4::Tle* p)  const noexcept; };
    struct Sgp4Deleter { void operator()(libsgp4::SGP4* p) const noexcept; };

    std::unique_ptr<libsgp4::Tle, TleDeleter>   tle_;
    std::unique_ptr<libsgp4::SGP4, Sgp4Deleter> sgp4_;
    double epochJd_ = 0.0;
    bool   ready_   = false;
};
