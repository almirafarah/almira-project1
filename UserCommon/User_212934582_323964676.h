#pragma once

#include "Membership_212934582_323964676.h"
#include <string>
#include <memory>
#include <vector>
#include <optional>

namespace Membership_212934582_323964676 {

    class User {
    public:
        User(const std::string& username, const std::string& email);
        
        // Copy constructor and assignment operator
        User(const User& other);
        User& operator=(const User& other);
        
        // Move constructor and assignment operator
        User(User&& other) = default;
        User& operator=(User&& other) = default;
        
        virtual ~User() = default;
        
        // Getters
        const std::string& getUsername() const { return username_; }
        const std::string& getEmail() const { return email_; }
        std::optional<Membership> getCurrentMembership() const;
        const std::vector<Membership>& getMembershipHistory() const { return membershipHistory_; }
        
        // Membership operations
        void addMembership(MembershipType type);
        bool hasActiveMembership() const;
        bool hasActiveProMembership() const;
        bool cancelCurrentMembership();
        bool cancelProMembership();
        
        // User management
        void updateEmail(const std::string& email) { email_ = email; }
        
        // Serialization support
        std::string toString() const;
        static std::optional<User> fromString(const std::string& str);
        
    private:
        std::string username_;
        std::string email_;
        std::vector<Membership> membershipHistory_;
        
        size_t findActiveMembershipIndex() const;
    };

} // namespace Membership_212934582_323964676