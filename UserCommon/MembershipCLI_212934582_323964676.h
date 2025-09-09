#pragma once

#include "../UserCommon/MembershipManager_212934582_323964676.h"
#include <string>
#include <vector>

namespace Membership_212934582_323964676 {

    class MembershipCLI {
    public:
        MembershipCLI();
        virtual ~MembershipCLI() = default;
        
        // Main CLI entry point
        int run(int argc, char* argv[]);
        
        // Individual commands
        bool handleCreateUser(const std::vector<std::string>& args);
        bool handleSubscribe(const std::vector<std::string>& args);
        bool handleCancelMembership(const std::vector<std::string>& args);
        bool handleCancelProMembership(const std::vector<std::string>& args);
        bool handleShowUser(const std::vector<std::string>& args);
        bool handleListUsers(const std::vector<std::string>& args);
        bool handleHelp(const std::vector<std::string>& args);
        
        // Interactive mode
        void runInteractive();
        
    private:
        MembershipManager manager_;
        
        void printUsage() const;
        void printHelp() const;
        std::vector<std::string> parseArgs(int argc, char* argv[]);
        std::string trim(const std::string& str) const;
        std::vector<std::string> split(const std::string& str, char delimiter) const;
        
        MembershipType parseMembershipType(const std::string& typeStr) const;
        std::string membershipTypeToString(MembershipType type) const;
    };

} // namespace Membership_212934582_323964676