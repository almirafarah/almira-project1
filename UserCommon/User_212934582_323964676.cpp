#include "User_212934582_323964676.h"
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace Membership_212934582_323964676 {

    User::User(const std::string& username, const std::string& email)
        : username_(username), email_(email) {
        if (username.empty()) {
            throw std::invalid_argument("Username cannot be empty");
        }
        if (email.empty()) {
            throw std::invalid_argument("Email cannot be empty");
        }
    }

    User::User(const User& other)
        : username_(other.username_), email_(other.email_), membershipHistory_(other.membershipHistory_) {
    }

    User& User::operator=(const User& other) {
        if (this != &other) {
            username_ = other.username_;
            email_ = other.email_;
            membershipHistory_ = other.membershipHistory_;
        }
        return *this;
    }

    std::optional<Membership> User::getCurrentMembership() const {
        auto index = findActiveMembershipIndex();
        if (index != membershipHistory_.size()) {
            return membershipHistory_[index];
        }
        return std::nullopt;
    }

    void User::addMembership(MembershipType type) {
        // Cancel any existing active membership before adding new one
        cancelCurrentMembership();
        
        auto now = std::chrono::system_clock::now();
        membershipHistory_.emplace_back(type, now);
    }

    bool User::hasActiveMembership() const {
        return findActiveMembershipIndex() != membershipHistory_.size();
    }

    bool User::hasActiveProMembership() const {
        auto index = findActiveMembershipIndex();
        if (index != membershipHistory_.size()) {
            return membershipHistory_[index].getType() == MembershipType::PRO;
        }
        return false;
    }

    bool User::cancelCurrentMembership() {
        auto index = findActiveMembershipIndex();
        if (index != membershipHistory_.size()) {
            membershipHistory_[index].cancel();
            return true;
        }
        return false;
    }

    bool User::cancelProMembership() {
        // Find active Pro membership and cancel it
        for (auto& membership : membershipHistory_) {
            if (membership.isActive() && membership.getType() == MembershipType::PRO) {
                membership.cancel();
                return true;
            }
        }
        return false;
    }

    std::string User::toString() const {
        std::ostringstream oss;
        oss << username_ << "|" << email_ << "|" << membershipHistory_.size();
        
        for (const auto& membership : membershipHistory_) {
            oss << "|" << static_cast<int>(membership.getType())
                << "|" << std::chrono::duration_cast<std::chrono::seconds>(
                    membership.getStartDate().time_since_epoch()).count()
                << "|" << std::chrono::duration_cast<std::chrono::seconds>(
                    membership.getEndDate().time_since_epoch()).count()
                << "|" << (membership.isCanceled() ? 1 : 0);
        }
        
        return oss.str();
    }

    std::optional<User> User::fromString(const std::string& str) {
        std::istringstream iss(str);
        std::string token;
        std::vector<std::string> tokens;
        
        while (std::getline(iss, token, '|')) {
            tokens.push_back(token);
        }
        
        if (tokens.size() < 3) {
            return std::nullopt;
        }
        
        try {
            std::string username = tokens[0];
            std::string email = tokens[1];
            size_t membershipCount = std::stoul(tokens[2]);
            
            User user(username, email);
            
            // Each membership needs 4 fields: type, start, end, canceled
            if (tokens.size() != 3 + membershipCount * 4) {
                return std::nullopt;
            }
            
            for (size_t i = 0; i < membershipCount; ++i) {
                size_t baseIndex = 3 + i * 4;
                int typeInt = std::stoi(tokens[baseIndex]);
                auto startTime = std::chrono::system_clock::time_point(
                    std::chrono::seconds(std::stoll(tokens[baseIndex + 1])));
                auto endTime = std::chrono::system_clock::time_point(
                    std::chrono::seconds(std::stoll(tokens[baseIndex + 2])));
                bool canceled = std::stoi(tokens[baseIndex + 3]) != 0;
                
                MembershipType type = static_cast<MembershipType>(typeInt);
                Membership membership(type, startTime);
                if (canceled) {
                    membership.cancel();
                }
                
                user.membershipHistory_.push_back(membership);
            }
            
            return user;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    size_t User::findActiveMembershipIndex() const {
        for (size_t i = 0; i < membershipHistory_.size(); ++i) {
            if (membershipHistory_[i].isActive()) {
                return i;
            }
        }
        return membershipHistory_.size(); // Not found
    }

} // namespace Membership_212934582_323964676