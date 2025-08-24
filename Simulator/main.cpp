#include "Simulator.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// ----- platform dynamic lib extension -----
#if defined(_WIN32)
#  define DYN_LIB_EXT ".dll"
#else
#  define DYN_LIB_EXT ".so"
#endif

// ------------------------------------------------------------------
// Usage printer
// ------------------------------------------------------------------
static void printUsage(const char* prog, const std::string& err = {}) {
    if (!err.empty()) {
        std::cerr << "Error: " << err << "\n\n";
    }
    std::cerr
        << "Usage:\n"
        << "  " << prog << " -comparative "
        << "game_map=<path.txt> game_managers_folder=<dir> algorithm1=<file" DYN_LIB_EXT "> algorithm2=<file" DYN_LIB_EXT ">"
        << " [num_threads=<N>] [-verbose]\n\n"
        << "  " << prog << " -competition "
        << "game_maps_folder=<dir> game_manager=<file" DYN_LIB_EXT "> algorithms_folder=<dir>"
        << " [num_threads=<N>] [-verbose]\n\n"
        << "Notes:\n"
        << "  - Paths can be relative to the current working directory.\n"
        << "  - On Windows, dynamic libraries use " DYN_LIB_EXT "; on Linux/macOS, they use " DYN_LIB_EXT ".\n";
}

// ------------------------------------------------------------------
// Helpers to split "name=value" tokens
// ------------------------------------------------------------------
std::string getArgumentValue(const std::string& arg) {
    size_t pos = arg.find('=');
    if (pos == std::string::npos) return {};

    // skip '='
    ++pos;
    // skip spaces after '='
    while (pos < arg.size() && std::isspace(static_cast<unsigned char>(arg[pos]))) {
        ++pos;
    }
    return arg.substr(pos);
}

std::string getArgumentName(const std::string& arg) {
    size_t pos = arg.find('=');
    if (pos == std::string::npos) return arg;

    // trim spaces before '='
    while (pos > 0 && std::isspace(static_cast<unsigned char>(arg[pos - 1]))) {
        --pos;
    }
    return arg.substr(0, pos);
}

// ------------------------------------------------------------------
// Argument parsing (now mode can be anywhere, unknown args collected)
// ------------------------------------------------------------------
bool parseArguments(int argc, char* argv[], std::map<std::string, std::string>& args,
                    bool& verbose, int& num_threads) {
    if (argc < 2) {
        printUsage(argv[0], "No arguments provided");
        return false;
    }

    // detect mode anywhere
    int mode_index = -1;
    for (int i = 1; i < argc; ++i) {
        std::string token = argv[i];
        if (token == "-comparative" || token == "-competition") {
            mode_index = i;
            args["mode"] = token;
            break;
        }
    }
    if (mode_index == -1) {
        printUsage(argv[0], "Missing mode: -comparative or -competition");
        return false;
    }

    verbose = false;
    num_threads = 1;

    // required args per mode
    std::vector<std::string> required_args;
    if (args["mode"] == "-comparative") {
        required_args = {"game_map", "game_managers_folder", "algorithm1", "algorithm2"};
    } else { // -competition
        required_args = {"game_maps_folder", "game_manager", "algorithms_folder"};
    }

    std::vector<std::string> unknown_args;

    // parse all args except program name and the mode flag itself
    for (int i = 1; i < argc; ++i) {
        if (i == mode_index) continue;

        std::string arg = argv[i];

        if (arg == "-verbose") {
            verbose = true;
            continue;
        }

        std::string name  = getArgumentName(arg);
        std::string value = getArgumentValue(arg);

        if (name == "num_threads") {
            try {
                num_threads = std::stoi(value);
                if (num_threads < 1) {
                    printUsage(argv[0], "num_threads must be >= 1");
                    return false;
                }
            } catch (...) {
                printUsage(argv[0], "Invalid num_threads value: " + value);
                return false;
            }
            continue;
        }

        bool is_valid = false;
        for (const auto& req : required_args) {
            if (name == req) {
                is_valid = true;
                args[name] = value;
                break;
            }
        }
        if (!is_valid) {
            unknown_args.push_back(name);
        }
    }

    if (!unknown_args.empty()) {
        std::string msg;
        for (size_t i = 0; i < unknown_args.size(); ++i) {
            if (i) msg += ", ";
            msg += unknown_args[i];
        }
        printUsage(argv[0], "Unknown argument(s): " + msg);
        return false;
    }

    // ensure required provided
    std::string missing;
    for (const auto& req : required_args) {
        if (args.find(req) == args.end()) {
            if (!missing.empty()) missing += ", ";
            missing += req;
        }
    }
    if (!missing.empty()) {
        printUsage(argv[0], "Missing required argument(s): " + missing);
        return false;
    }

    return true;
}

// ------------------------------------------------------------------
// Path validation (existence + minimal content with expected ext)
// ------------------------------------------------------------------
bool validatePaths(const std::map<std::string, std::string>& args, const char* program_name) {
    namespace fs = std::filesystem;

    auto requireFile = [&](const std::string& path, const std::string& msg) -> bool {
        if (!fs::exists(path) || !fs::is_regular_file(path)) {
            printUsage(program_name, msg + path);
            return false;
        }
        return true;
    };

    auto requireDirWithExt = [&](const std::string& dir, const std::string& ext,
                                 const std::string& msg, size_t min_files = 1) -> bool {
        if (!fs::exists(dir) || !fs::is_directory(dir)) {
            printUsage(program_name, msg + dir);
            return false;
        }
        size_t count = 0;
        for (const auto& e : fs::directory_iterator(dir)) {
            if (e.is_regular_file() && e.path().extension() == ext) ++count;
        }
        if (count < min_files) {
            printUsage(program_name, "No files with extension " + ext + " found in " + dir);
            return false;
        }
        return true;
    };

    if (args.at("mode") == "-comparative") {
        if (!requireFile(args.at("game_map"), "map file not found: ")) return false;
        if (!requireDirWithExt(args.at("game_managers_folder"), DYN_LIB_EXT,
                               "Invalid game_managers_folder: ")) return false;
        if (!requireFile(args.at("algorithm1"), "algorithm1 not found: ")) return false;
        if (!requireFile(args.at("algorithm2"), "algorithm2 not found: ")) return false;
    } else { // -competition
        if (!requireDirWithExt(args.at("game_maps_folder"), ".txt",
                               "Invalid game_maps_folder: ")) return false;
        if (!requireFile(args.at("game_manager"), "game_manager not found: ")) return false;
        if (!requireDirWithExt(args.at("algorithms_folder"), DYN_LIB_EXT,
                               "Invalid algorithms_folder: ", 2)) return false;
    }
    return true;
}

// ------------------------------------------------------------------
// main
// ------------------------------------------------------------------
int main(int argc, char* argv[]) {
    std::cout << "Tank Battle Simulator Starting..." << std::endl;

    // show cwd to help with relative paths
    try {
        std::cout << "CWD: " << std::filesystem::current_path().string() << std::endl;
    } catch (...) {
        // ignore
    }

    std::map<std::string, std::string> args;
    bool verbose = false;
    int num_threads = 1;

    if (!parseArguments(argc, argv, args, verbose, num_threads) ||
        !validatePaths(args, argv[0])) {
        return 1;
    }

    try {
        Simulator simulator;

        bool ok = false;
        if (args["mode"] == "-comparative") {
            ok = simulator.runComparative(
                args.at("game_map"),
                args.at("game_managers_folder"),
                args.at("algorithm1"),
                args.at("algorithm2"),
                num_threads,
                verbose
            );
        } else {
            ok = simulator.runCompetition(
                args.at("game_maps_folder"),
                args.at("game_manager"),
                args.at("algorithms_folder"),
                num_threads,
                verbose
            );
        }

        std::cout << (ok ? "Simulation completed successfully!" : "Simulation failed!") << std::endl;
        return ok ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
