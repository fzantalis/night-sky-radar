#include "Propagator.h"

#include <string>
#include <utility>

#include "TimeUtils.h"

#include "sgp4/SGP4.h"
#include "sgp4/Tle.h"

Propagator::Propagator() = default;
Propagator::~Propagator() = default;
Propagator::Propagator(Propagator&&) noexcept = default;
Propagator& Propagator::operator=(Propagator&&) noexcept = default;

bool Propagator::init(const Tle& tle) {
    ready_ = false;
    epochJd_ = 0.0;

    try {
        auto libTle = std::make_unique<libsgp4::Tle>(
            std::string(tle.name), std::string(tle.line1), std::string(tle.line2));
        auto libSgp4 = std::make_unique<libsgp4::SGP4>(*libTle);

        epochJd_ = libTle->Epoch().ToJulian();
        tle_ = std::move(libTle);
        sgp4_ = std::move(libSgp4);
        ready_ = true;
        return true;
    } catch (...) {
        tle_.reset();
        sgp4_.reset();
        epochJd_ = 0.0;
        ready_ = false;
        return false;
    }
}

bool Propagator::positionAt(int64_t unixSeconds, Vec3& posKm) const {
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
