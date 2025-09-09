#pragma once

#include "User_212934582_323964676.h"
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace Membership_212934582_323964676 {

    class MembershipManager {
    public:
        MembershipManager();
        virtual ~MembershipManager() = default;
        
        // User management
        bool createUser(const std::string& username, const std::string& email);
        std::optional<User> getUser(const std::string& username) const;
        bool userExists(const std::string& username) const;
        std::vector<std::string> getAllUsernames() const;
        
        // Membership operations
        bool subscribeTo(const std::string& username, MembershipType type);
        bool cancelMembership(const std::string& username);
        bool cancelProMembership(const std::string& username);
        
        // Queries
        std::vector<std::string> getUsersWithActiveMembership() const;
        std::vector<std::string> getUsersWithProMembership() const;
        
        // Persistence
        bool saveToFile(const std::string& filename) const;
        bool loadFromFile(const std::string& filename);
        
        // Utilities
        size_t getUserCount() const { return users_.size(); }
        void clearAllUsers(); // For testing
        
        // Display
        std::string getMembershipStatusReport(const std::string& username) const;
        std::string getAllUsersReport() const;
        
    private:
        std::map<std::string, User> users_;
        
        static const std::string DATA_FILE_DEFAULT;
        
        bool updateUser(const User& user);
    };

} // namespace Membership_212934582_323964676