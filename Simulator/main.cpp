#include "Simulator.h"
#include <iostream>
#include <string>
#include <map>
#include <filesystem>

// Platform-dependent dynamic library extension used in help messages
#ifdef _WIN32
static const char* DYN_LIB_EXT = ".dll";
#else
static const char* DYN_LIB_EXT = ".so";
#endif

void printUsage(const std::string& program_name, const std::string& error_msg = "") {
    std::cout << "Usage:" << std::endl;
    std::cout << "  " << program_name
              << " -comparative game_map=<file> game_managers_folder=<folder> algorithm1=<" << DYN_LIB_EXT
              << "> algorithm2=<" << DYN_LIB_EXT << "> [num_threads=<N>] [-verbose]" << std::endl;
    std::cout << "  " << program_name
              << " -competition game_maps_folder=<folder> game_manager=<" << DYN_LIB_EXT
              << "> algorithms_folder=<folder> [num_threads=<N>] [-verbose]" << std::endl;
    std::cout << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  -comparative: Run all GameManagers in folder on single map with two algorithms" << std::endl;
    std::cout << "  -competition: Run one GameManager on all maps with tournament of algorithms" << std::endl;
    std::cout << "  num_threads: Number of worker threads (optional, default=1)" << std::endl;
    std::cout << "  -verbose: Enable verbose output (optional)" << std::endl;
    std::cout << std::endl;
    std::cout << "Notes:" << std::endl;
    std::cout << "  - All arguments can appear in any order" << std::endl;
    std::cout << "  - All arguments except num_threads and -verbose are mandatory" << std::endl;
    std::cout << "  - The = sign can appear with any number of spaces around it" << std::endl;
    
    if (!error_msg.empty()) {
        std::cout << std::endl;
        std::cout << "Error: " << error_msg << std::endl;
    }
}

std::string getArgumentValue(const std::string& arg) {
    // Find the = sign
    size_t pos = arg.find('=');
    if (pos == std::string::npos) {
        return "";
    }
    
    // Skip any spaces after the = sign
    pos++;
    while (pos < arg.length() && std::isspace(arg[pos])) {
        pos++;
    }
    
    return arg.substr(pos);
}

std::string getArgumentName(const std::string& arg) {
    // Find the = sign
    size_t pos = arg.find('=');
    if (pos == std::string::npos) {
        return arg;
    }
    
    // Trim trailing spaces before the = sign
    while (pos > 0 && std::isspace(arg[pos - 1])) {
        pos--;
    }
    
    return arg.substr(0, pos);
}

bool parseArguments(int argc, char* argv[], std::map<std::string, std::string>& args, bool& verbose, int& num_threads) {
    if (argc < 2) {
        printUsage(argv[0], "No arguments provided");
        return false;
    }
    
    std::string mode = argv[1];
    if (mode != "-comparative" && mode != "-competition") {
        printUsage(argv[0], "Invalid mode: " + mode + ". Must be -comparative or -competition");
        return false;
    }
    
    args["mode"] = mode;
    verbose = false;
    num_threads = 1;
    
    // Required arguments for each mode
    std::vector<std::string> required_args;
    if (mode == "-comparative") {
        required_args = {"game_map", "game_managers_folder", "algorithm1", "algorithm2"};
    } else {
        required_args = {"game_maps_folder", "game_manager", "algorithms_folder"};
    }
    
    // Parse all arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        
        // Handle -verbose flag
        if (arg == "-verbose") {
            verbose = true;
            continue;
        }
        
        // Get argument name and value
        std::string name = getArgumentName(arg);
        std::string value = getArgumentValue(arg);
        
        // Handle num_threads
        if (name == "num_threads") {
            try {
                num_threads = std::stoi(value);
                if (num_threads < 1) {
                    printUsage(argv[0], "num_threads must be >= 1");
                    return false;
                }
            } catch (const std::exception& e) {
                printUsage(argv[0], "Invalid num_threads value: " + value);
                return false;
            }
            continue;
        }
        
        // Check if this is a valid argument for current mode
        bool is_valid = false;
        for (const auto& req : required_args) {
            if (name == req) {
                is_valid = true;
                args[name] = value;
                break;
            }
        }
        
        if (!is_valid) {
            printUsage(argv[0], "Unknown argument: " + name);
            return false;
        }
    }
    
    // Check that all required arguments were provided
    std::string missing_args;
    for (const auto& req : required_args) {
        if (args.find(req) == args.end()) {
            if (!missing_args.empty()) missing_args += ", ";
            missing_args += req;
        }
    }
    
    if (!missing_args.empty()) {
        printUsage(argv[0], "Missing required argument(s): " + missing_args);
        return false;
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    std::cout << "Tank Battle Simulator Starting..." << std::endl;
    // Debug: show current working directory so users can reason about relative paths
    try {
        std::cout << "CWD: " << std::filesystem::current_path().string() << std::endl;
    } catch (...) {
        // ignore
    }
    
    // Parse command line arguments
    std::map<std::string, std::string> args;
    bool verbose;
    int num_threads;
    
    if (!parseArguments(argc, argv, args, verbose, num_threads)) {
        return 1;
    }
    
    try {
        // Create simulator
        Simulator simulator;
        
        // Run in appropriate mode
        bool success = false;
        if (args["mode"] == "-comparative") {
            // Quick existence checks to help diagnose path issues on different working dirs
            std::cout << "Path checks (comparative):" << std::endl;
            std::cout << "  game_map exists: " << std::boolalpha << std::filesystem::exists(args["game_map"]) 
                      << " (" << args["game_map"] << ")" << std::endl;
            std::cout << "  game_managers_folder exists: " << std::boolalpha << std::filesystem::exists(args["game_managers_folder"]) 
                      << " (" << args["game_managers_folder"] << ")" << std::endl;
            std::cout << "  algorithm1 exists: " << std::boolalpha << std::filesystem::exists(args["algorithm1"]) 
                      << " (" << args["algorithm1"] << ")" << std::endl;
            std::cout << "  algorithm2 exists: " << std::boolalpha << std::filesystem::exists(args["algorithm2"]) 
                      << " (" << args["algorithm2"] << ")" << std::endl;
            success = simulator.runComparative(
                args["game_map"],
                args["game_managers_folder"],
                args["algorithm1"],
                args["algorithm2"],
                num_threads,
                verbose
            );
        } else if (args["mode"] == "-competition") {
            std::cout << "Path checks (competition):" << std::endl;
            std::cout << "  game_maps_folder exists: " << std::boolalpha << std::filesystem::exists(args["game_maps_folder"]) 
                      << " (" << args["game_maps_folder"] << ")" << std::endl;
            std::cout << "  game_manager exists: " << std::boolalpha << std::filesystem::exists(args["game_manager"]) 
                      << " (" << args["game_manager"] << ")" << std::endl;
            std::cout << "  algorithms_folder exists: " << std::boolalpha << std::filesystem::exists(args["algorithms_folder"]) 
                      << " (" << args["algorithms_folder"] << ")" << std::endl;
            success = simulator.runCompetition(
                args["game_maps_folder"],
                args["game_manager"],
                args["algorithms_folder"],
                num_threads,
                verbose
            );
        }
        
        if (success) {
            std::cout << "Simulation completed successfully!" << std::endl;
            return 0;
        } else {
            std::cout << "Simulation failed!" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
