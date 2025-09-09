#include "MembershipManager_212934582_323964676.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace Membership_212934582_323964676 {

    const std::string MembershipManager::DATA_FILE_DEFAULT = "users.dat";

    MembershipManager::MembershipManager() {
        // Try to load existing data
        loadFromFile(DATA_FILE_DEFAULT);
    }

    bool MembershipManager::createUser(const std::string& username, const std::string& email) {
        if (userExists(username)) {
            return false; // User already exists
        }
        
        try {
            User newUser(username, email);
            users_.emplace(username, std::move(newUser));
            saveToFile(DATA_FILE_DEFAULT); // Auto-save
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    std::optional<User> MembershipManager::getUser(const std::string& username) const {
        auto it = users_.find(username);
        if (it != users_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool MembershipManager::userExists(const std::string& username) const {
        return users_.find(username) != users_.end();
    }

    std::vector<std::string> MembershipManager::getAllUsernames() const {
        std::vector<std::string> usernames;
        usernames.reserve(users_.size());
        
        for (const auto& pair : users_) {
            usernames.push_back(pair.first);
        }
        
        std::sort(usernames.begin(), usernames.end());
        return usernames;
    }

    bool MembershipManager::subscribeTo(const std::string& username, MembershipType type) {
        auto it = users_.find(username);
        if (it == users_.end()) {
            return false; // User doesn't exist
        }
        
        it->second.addMembership(type);
        saveToFile(DATA_FILE_DEFAULT); // Auto-save
        return true;
    }

    bool MembershipManager::cancelMembership(const std::string& username) {
        auto it = users_.find(username);
        if (it == users_.end()) {
            return false; // User doesn't exist
        }
        
        bool success = it->second.cancelCurrentMembership();
        if (success) {
            saveToFile(DATA_FILE_DEFAULT); // Auto-save
        }
        return success;
    }

    bool MembershipManager::cancelProMembership(const std::string& username) {
        auto it = users_.find(username);
        if (it == users_.end()) {
            return false; // User doesn't exist
        }
        
        bool success = it->second.cancelProMembership();
        if (success) {
            saveToFile(DATA_FILE_DEFAULT); // Auto-save
        }
        return success;
    }

    std::vector<std::string> MembershipManager::getUsersWithActiveMembership() const {
        std::vector<std::string> result;
        
        for (const auto& pair : users_) {
            if (pair.second.hasActiveMembership()) {
                result.push_back(pair.first);
            }
        }
        
        std::sort(result.begin(), result.end());
        return result;
    }

    std::vector<std::string> MembershipManager::getUsersWithProMembership() const {
        std::vector<std::string> result;
        
        for (const auto& pair : users_) {
            if (pair.second.hasActiveProMembership()) {
                result.push_back(pair.first);
            }
        }
        
        std::sort(result.begin(), result.end());
        return result;
    }

    bool MembershipManager::saveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        for (const auto& pair : users_) {
            file << pair.second.toString() << std::endl;
        }
        
        return file.good();
    }

    bool MembershipManager::loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false; // File doesn't exist, that's okay for first run
        }
        
        users_.clear();
        std::string line;
        
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            auto user = User::fromString(line);
            if (user) {
                users_.emplace(user->getUsername(), std::move(*user));
            }
        }
        
        return true;
    }

    void MembershipManager::clearAllUsers() {
        users_.clear();
        saveToFile(DATA_FILE_DEFAULT);
    }

    std::string MembershipManager::getMembershipStatusReport(const std::string& username) const {
        auto user = getUser(username);
        if (!user) {
            return "User '" + username + "' not found.";
        }
        
        std::ostringstream oss;
        oss << "User: " << user->getUsername() << std::endl;
        oss << "Email: " << user->getEmail() << std::endl;
        
        auto currentMembership = user->getCurrentMembership();
        if (currentMembership) {
            oss << "Current Membership: " << currentMembership->getTypeString() 
                << " (" << currentMembership->getStatusString() << ")" << std::endl;
        } else {
            oss << "Current Membership: None" << std::endl;
        }
        
        const auto& history = user->getMembershipHistory();
        if (!history.empty()) {
            oss << "Membership History:" << std::endl;
            for (const auto& membership : history) {
                oss << "  - " << membership.getTypeString() 
                    << " (" << membership.getStatusString() << ")" << std::endl;
            }
        }
        
        return oss.str();
    }

    std::string MembershipManager::getAllUsersReport() const {
        std::ostringstream oss;
        oss << "=== All Users Report ===" << std::endl;
        oss << "Total Users: " << users_.size() << std::endl;
        
        auto activeUsers = getUsersWithActiveMembership();
        auto proUsers = getUsersWithProMembership();
        
        oss << "Users with Active Memberships: " << activeUsers.size() << std::endl;
        oss << "Users with Pro Memberships: " << proUsers.size() << std::endl;
        oss << std::endl;
        
        for (const auto& username : getAllUsernames()) {
            auto user = getUser(username);
            if (user) {
                oss << "User: " << username;
                auto membership = user->getCurrentMembership();
                if (membership) {
                    oss << " - " << membership->getTypeString() 
                        << " (" << membership->getStatusString() << ")";
                } else {
                    oss << " - No active membership";
                }
                oss << std::endl;
            }
        }
        
        return oss.str();
    }

    bool MembershipManager::updateUser(const User& user) {
        auto it = users_.find(user.getUsername());
        if (it != users_.end()) {
            it->second = user;
        } else {
            users_.emplace(user.getUsername(), user);
        }
        return saveToFile(DATA_FILE_DEFAULT);
    }

} // namespace Membership_212934582_323964676