#include "Propagator.h"

#include <new>
#include <string>
#include <utility>

#include "TimeUtils.h"

#include "sgp4/SGP4.h"
#include "sgp4/Tle.h"

void Propagator::TleDeleter::operator()(libsgp4::Tle* p) const noexcept {
    if (p != nullptr) {
        p->~Tle();
        corealloc::free(p);
    }
}

void Propagator::Sgp4Deleter::operator()(libsgp4::SGP4* p) const noexcept {
    if (p != nullptr) {
        p->~SGP4();
        corealloc::free(p);
    }
}

Propagator::Propagator() = default;
Propagator::~Propagator() = default;

Propagator::Propagator(Propagator&& other) noexcept
    : tle_(std::move(other.tle_)),
      sgp4_(std::move(other.sgp4_)),
      epochJd_(other.epochJd_),
      ready_(other.ready_) {
    // Leave the moved-from object in a well-defined "not ready" state.
    // `= default` would only null the unique_ptrs, leaving ready_/epochJd_
    // stale (copied, not cleared) so a moved-from Propagator would still
    // claim ready() == true with no propagator behind it.
    other.ready_ = false;
    other.epochJd_ = 0.0;
}

Propagator& Propagator::operator=(Propagator&& other) noexcept {
    if (this != &other) {
        tle_ = std::move(other.tle_);
        sgp4_ = std::move(other.sgp4_);
        epochJd_ = other.epochJd_;
        ready_ = other.ready_;

        other.ready_ = false;
        other.epochJd_ = 0.0;
    }
    return *this;
}

bool Propagator::init(const Tle& tle) {
    ready_ = false;
    epochJd_ = 0.0;

    // Drop any previous payload up front so a failed re-init never leaves a
    // stale allocation attached to this instance.
    tle_.reset();
    sgp4_.reset();

    void* tleMem = corealloc::alloc(sizeof(libsgp4::Tle));
    if (tleMem == nullptr) {
        return false;
    }

    libsgp4::Tle* rawTle = nullptr;
    try {
        rawTle = new (tleMem) libsgp4::Tle(
            std::string(tle.name), std::string(tle.line1), std::string(tle.line2));
    } catch (...) {
        // Construction threw: no object was ever completed at tleMem, so
        // free the raw memory directly. Running it through TleDeleter here
        // would call ~Tle() on a non-existent object.
        corealloc::free(tleMem);
        return false;
    }
    std::unique_ptr<libsgp4::Tle, TleDeleter> libTle(rawTle);

    void* sgp4Mem = corealloc::alloc(sizeof(libsgp4::SGP4));
    if (sgp4Mem == nullptr) {
        return false;   // libTle goes out of scope: destructs + frees itself
    }

    libsgp4::SGP4* rawSgp4 = nullptr;
    try {
        rawSgp4 = new (sgp4Mem) libsgp4::SGP4(*libTle);
    } catch (...) {
        corealloc::free(sgp4Mem);
        return false;
    }
    std::unique_ptr<libsgp4::SGP4, Sgp4Deleter> libSgp4(rawSgp4);

    try {
        epochJd_ = libTle->Epoch().ToJulian();
    } catch (...) {
        epochJd_ = 0.0;
        return false;
    }

    tle_ = std::move(libTle);
    sgp4_ = std::move(libSgp4);
    ready_ = true;
    return true;
}

bool Propagator::positionAt(int64_t unixSeconds, Vec3& posKm) {
    if (!ready_ || !sgp4_) {
        return false;
    }

    try {
        const double tsinceMinutes =
            (timeutils::julianDate(unixSeconds) - epochJd_) * 1440.0;
        const libsgp4::Eci eci = sgp4_->FindPosition(tsinceMinutes);
        const libsgp4::Vector pos = eci.Position();

        posKm.x = pos.x;
        posKm.y = pos.y;
        posKm.z = pos.z;
        return true;
    } catch (...) {
        return false;
    }
}
