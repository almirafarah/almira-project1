#include "MembershipCLI_212934582_323964676.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace Membership_212934582_323964676 {

    MembershipCLI::MembershipCLI() {
        // Constructor - manager auto-loads data
    }

    int MembershipCLI::run(int argc, char* argv[]) {
        auto args = parseArgs(argc, argv);
        
        if (args.empty()) {
            std::cout << "Starting interactive membership management..." << std::endl;
            runInteractive();
            return 0;
        }
        
        std::string command = args[0];
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);
        
        bool success = false;
        
        if (command == "create" || command == "create-user") {
            success = handleCreateUser(args);
        } else if (command == "subscribe") {
            success = handleSubscribe(args);
        } else if (command == "cancel") {
            success = handleCancelMembership(args);
        } else if (command == "cancel-pro") {
            success = handleCancelProMembership(args);
        } else if (command == "show" || command == "user") {
            success = handleShowUser(args);
        } else if (command == "list" || command == "list-users") {
            success = handleListUsers(args);
        } else if (command == "help" || command == "--help" || command == "-h") {
            success = handleHelp(args);
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            printUsage();
            return 1;
        }
        
        return success ? 0 : 1;
    }

    bool MembershipCLI::handleCreateUser(const std::vector<std::string>& args) {
        if (args.size() != 3) {
            std::cerr << "Usage: create <username> <email>" << std::endl;
            return false;
        }
        
        const std::string& username = args[1];
        const std::string& email = args[2];
        
        if (manager_.createUser(username, email)) {
            std::cout << "User '" << username << "' created successfully." << std::endl;
            return true;
        } else {
            std::cerr << "Failed to create user. User may already exist or invalid data provided." << std::endl;
            return false;
        }
    }

    bool MembershipCLI::handleSubscribe(const std::vector<std::string>& args) {
        if (args.size() != 3) {
            std::cerr << "Usage: subscribe <username> <basic|pro|premium>" << std::endl;
            return false;
        }
        
        const std::string& username = args[1];
        const std::string& typeStr = args[2];
        
        MembershipType type = parseMembershipType(typeStr);
        if (type == static_cast<MembershipType>(-1)) {
            std::cerr << "Invalid membership type. Use: basic, pro, or premium" << std::endl;
            return false;
        }
        
        if (manager_.subscribeTo(username, type)) {
            std::cout << "User '" << username << "' subscribed to " 
                      << membershipTypeToString(type) << " membership." << std::endl;
            return true;
        } else {
            std::cerr << "Failed to subscribe user. User may not exist." << std::endl;
            return false;
        }
    }

    bool MembershipCLI::handleCancelMembership(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            std::cerr << "Usage: cancel <username>" << std::endl;
            return false;
        }
        
        const std::string& username = args[1];
        
        if (manager_.cancelMembership(username)) {
            std::cout << "Membership canceled for user '" << username << "'." << std::endl;
            return true;
        } else {
            std::cerr << "Failed to cancel membership. User may not exist or have no active membership." << std::endl;
            return false;
        }
    }

    bool MembershipCLI::handleCancelProMembership(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            std::cerr << "Usage: cancel-pro <username>" << std::endl;
            return false;
        }
        
        const std::string& username = args[1];
        
        // Check if user has pro membership first
        auto user = manager_.getUser(username);
        if (!user) {
            std::cerr << "User '" << username << "' not found." << std::endl;
            return false;
        }
        
        if (!user->hasActiveProMembership()) {
            std::cerr << "User '" << username << "' does not have an active Pro membership." << std::endl;
            return false;
        }
        
        if (manager_.cancelProMembership(username)) {
            std::cout << "Pro membership canceled successfully for user '" << username << "'." << std::endl;
            std::cout << "Your Pro membership has been canceled. You will retain access until the end of your current billing period." << std::endl;
            return true;
        } else {
            std::cerr << "Failed to cancel Pro membership for user '" << username << "'." << std::endl;
            return false;
        }
    }

    bool MembershipCLI::handleShowUser(const std::vector<std::string>& args) {
        if (args.size() != 2) {
            std::cerr << "Usage: show <username>" << std::endl;
            return false;
        }
        
        const std::string& username = args[1];
        std::string report = manager_.getMembershipStatusReport(username);
        std::cout << report << std::endl;
        return true;
    }

    bool MembershipCLI::handleListUsers(const std::vector<std::string>& args) {
        std::string report = manager_.getAllUsersReport();
        std::cout << report << std::endl;
        return true;
    }

    bool MembershipCLI::handleHelp(const std::vector<std::string>& args) {
        printHelp();
        return true;
    }

    void MembershipCLI::runInteractive() {
        std::cout << "=== Membership Management System ===" << std::endl;
        std::cout << "Type 'help' for available commands or 'quit' to exit." << std::endl;
        
        std::string line;
        while (true) {
            std::cout << "\nmembership> ";
            if (!std::getline(std::cin, line)) {
                break; // EOF
            }
            
            line = trim(line);
            if (line.empty()) continue;
            
            if (line == "quit" || line == "exit") {
                std::cout << "Goodbye!" << std::endl;
                break;
            }
            
            auto tokens = split(line, ' ');
            if (tokens.empty()) continue;
            
            // Convert to char* argv format for compatibility
            std::vector<char*> argv;
            std::vector<std::string> argStrs = tokens;
            for (auto& arg : argStrs) {
                argv.push_back(&arg[0]);
            }
            argv.push_back(nullptr);
            
            run(static_cast<int>(tokens.size()), argv.data());
        }
    }

    std::vector<std::string> MembershipCLI::parseArgs(int argc, char* argv[]) {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) { // Skip program name
            args.emplace_back(argv[i]);
        }
        return args;
    }

    void MembershipCLI::printUsage() const {
        std::cout << "Membership Management Commands:" << std::endl;
        std::cout << "  create <username> <email>           - Create a new user" << std::endl;
        std::cout << "  subscribe <username> <type>         - Subscribe user to membership (basic|pro|premium)" << std::endl;
        std::cout << "  cancel <username>                   - Cancel current membership" << std::endl;
        std::cout << "  cancel-pro <username>               - Cancel Pro membership specifically" << std::endl;
        std::cout << "  show <username>                     - Show user membership status" << std::endl;
        std::cout << "  list                                - List all users" << std::endl;
        std::cout << "  help                                - Show this help" << std::endl;
    }

    void MembershipCLI::printHelp() const {
        std::cout << "=== Membership Management Help ===" << std::endl;
        printUsage();
        std::cout << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  create john john@example.com" << std::endl;
        std::cout << "  subscribe john pro" << std::endl;
        std::cout << "  cancel-pro john" << std::endl;
        std::cout << "  show john" << std::endl;
        std::cout << "  list" << std::endl;
        std::cout << std::endl;
        std::cout << "In interactive mode, you can also type 'quit' or 'exit' to leave." << std::endl;
    }

    std::string MembershipCLI::trim(const std::string& str) const {
        auto start = str.find_first_not_of(" \\t\\n\\r");
        if (start == std::string::npos) return "";
        
        auto end = str.find_last_not_of(" \\t\\n\\r");
        return str.substr(start, end - start + 1);
    }

    std::vector<std::string> MembershipCLI::split(const std::string& str, char delimiter) const {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        
        while (std::getline(ss, token, delimiter)) {
            token = trim(token);
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        
        return tokens;
    }

    MembershipType MembershipCLI::parseMembershipType(const std::string& typeStr) const {
        std::string lower = typeStr;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        if (lower == "basic") return MembershipType::BASIC;
        if (lower == "pro") return MembershipType::PRO;
        if (lower == "premium") return MembershipType::PREMIUM;
        
        return static_cast<MembershipType>(-1); // Invalid
    }

    std::string MembershipCLI::membershipTypeToString(MembershipType type) const {
        switch (type) {
            case MembershipType::BASIC: return "Basic";
            case MembershipType::PRO: return "Pro";
            case MembershipType::PREMIUM: return "Premium";
            default: return "Unknown";
        }
    }

} // namespace Membership_212934582_323964676