#include "Membership_212934582_323964676.h"
#include <stdexcept>

namespace Membership_212934582_323964676 {

    Membership::Membership(MembershipType type, const std::chrono::system_clock::time_point& startDate)
        : type_(type), startDate_(startDate), canceled_(false) {
        
        // Set default end date based on membership type (1 year from start)
        auto duration = std::chrono::hours(24 * 365); // 1 year
        endDate_ = startDate_ + duration;
    }

    bool Membership::isActive() const {
        if (canceled_) {
            return false;
        }
        
        auto now = std::chrono::system_clock::now();
        return now >= startDate_ && now <= endDate_;
    }

    void Membership::cancel() {
        canceled_ = true;
        // Set end date to now when canceled
        endDate_ = std::chrono::system_clock::now();
    }

    std::string Membership::getTypeString() const {
        switch (type_) {
            case MembershipType::BASIC:
                return "Basic";
            case MembershipType::PRO:
                return "Pro";
            case MembershipType::PREMIUM:
                return "Premium";
            default:
                return "Unknown";
        }
    }

    std::string Membership::getStatusString() const {
        if (canceled_) {
            return "Canceled";
        } else if (isActive()) {
            return "Active";
        } else {
            return "Expired";
        }
    }

} // namespace Membership_212934582_323964676