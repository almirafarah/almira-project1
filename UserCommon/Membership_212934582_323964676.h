#pragma once

#include <string>
#include <chrono>

namespace Membership_212934582_323964676 {

    enum class MembershipType {
        BASIC,
        PRO,
        PREMIUM
    };

    class Membership {
    public:
        Membership(MembershipType type, const std::chrono::system_clock::time_point& startDate);
        
        // Copy constructor and assignment operator
        Membership(const Membership& other) = default;
        Membership& operator=(const Membership& other) = default;
        
        // Move constructor and assignment operator  
        Membership(Membership&& other) = default;
        Membership& operator=(Membership&& other) = default;
        
        virtual ~Membership() = default;
        
        MembershipType getType() const { return type_; }
        std::chrono::system_clock::time_point getStartDate() const { return startDate_; }
        std::chrono::system_clock::time_point getEndDate() const { return endDate_; }
        bool isActive() const;
        void cancel();
        bool isCanceled() const { return canceled_; }
        
        std::string getTypeString() const;
        std::string getStatusString() const;
        
    private:
        MembershipType type_;
        std::chrono::system_clock::time_point startDate_;
        std::chrono::system_clock::time_point endDate_;
        bool canceled_;
    };

} // namespace Membership_212934582_323964676