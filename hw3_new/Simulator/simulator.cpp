#include "Simulator.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <dlfcn.h>
#include <unistd.h>   // getcwd
#include <limits.h>   // PATH_MAX
#include "AlgorithmRegistrar.h"
#include "GameManagerRegistrar.h"
#include <unordered_set>

namespace fs = std::filesystem;

// Static singleton accessor
Simulator& Simulator::getInstance() {
    static Simulator instance;
    return instance;
}

// Static registration callbacks: add factory to appropriate list
void Simulator::registerGameManagerFactory(GameManagerFactory factory) {
    Simulator::getInstance().gmFactories.push_back(std::move(factory));
}
void Simulator::registerPlayerFactory(PlayerFactory factory) {
    Simulator::getInstance().playerFactories.push_back(std::move(factory));
}
void Simulator::registerTankAlgorithmFactory(TankAlgorithmFactory factory) {
    Simulator::getInstance().tankFactories.push_back(std::move(factory));
}

// Helper: trim whitespace from both ends of a string (for parsing).
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? std::string() : s.substr(start, end - start + 1);
}

int Simulator::run(int argc, char* argv[]) {

    // Variables to store parsed values
    bool modeComparative = false;
    bool modeCompetition = false;
    bool verboseFlag = false;
    std::string gameMapFile;
    std::string gameMapsFolder;
    std::string gameManagersFolder;
    std::string gameManagerFile;
    std::string algorithm1File;
    std::string algorithm2File;
    std::string algorithmsFolder;
    int numThreads = 1; // default if not provided

    // Track errors in command-line parsing
    std::vector<std::string> unknownArgs;
    std::vector<std::string> missingParams;

    // Parse all arguments (order can be arbitrary):contentReference[oaicite:17]{index=17}
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        // Check for mode flags
        if (arg == "-comparative") {
            modeComparative = true;
            continue;
        }
        if (arg == "-competition") {
            modeCompetition = true;
            continue;
        }
        if (arg == "-verbose") {
            verboseFlag = true;
            continue;
        }

        // Handle key=value arguments (possibly with spaces around '=')
        std::string key, value;
        size_t eqPos = arg.find('=');
        
        if (eqPos == std::string::npos) {
        // Reject forms like: --key = value  (spaces around '=' are not allowed in A3)
            if (i + 2 < argc && std::string(argv[i+1]) == "=") {
                // Build a helpful error message
                std::string bad = std::string(argv[i]) + " " + argv[i+1] + " " + argv[i+2];
                std::string good = std::string(argv[i]) + "=" + argv[i+2];

                // Record as an unknown/invalid arg with a hint
                unknownArgs.push_back(bad + "  (invalid format; use '" + good + "')");

                // Skip the '=' and the value we just inspected
                i += 2;
            } else {
                // No '=' at all → invalid
                unknownArgs.push_back(std::string(argv[i]) +
                              "  (invalid format; expected '--key=value')");
            }
            continue; // move to next argv token

        } else {
            // Key and value might be in the same token, e.g. "key=val" or "key= val" etc.
            key = arg.substr(0, eqPos);
            value = arg.substr(eqPos + 1);

            // If value is empty → invalid (reject cases like "--key=" or "--key= value")
            if (value.empty()) {
                unknownArgs.push_back(arg + "  (invalid format; expected '--key=value' with no spaces)");
                continue;
            }
        }

        if (!key.empty()) {
            // Assign value to appropriate variable
            if (key == "game_map") {
                gameMapFile = value;
            } else if (key == "game_maps_folder") {
                gameMapsFolder = value;
            } else if (key == "game_managers_folder") {
                gameManagersFolder = value;
            } else if (key == "game_manager") {
                gameManagerFile = value;
            } else if (key == "algorithm1") {
                algorithm1File = value;
            } else if (key == "algorithm2") {
                algorithm2File = value;
            } else if (key == "algorithms_folder") {
                algorithmsFolder = value;
            } else if (key == "num_threads") {
                try {
                    numThreads = std::stoi(value);
                } catch (...) {
                    // If num_threads value is not a number, treat as unknown argument
                    unknownArgs.push_back(key + "=" + value);
                }
            } else {
                // If key not recognized
                unknownArgs.push_back(key + "=" + value);
            }
        }
    }

    // Determine mode validity
    if (modeComparative && modeCompetition) {
        // Both modes provided - this is unsupported
        unknownArgs.push_back("Both -comparative and -competition");
    } else if (!modeComparative && !modeCompetition) {
        // No mode provided
        missingParams.push_back("mode (-comparative or -competition)");
    }

    // Check required arguments based on mode
    if (modeComparative) {
        if (gameMapFile.empty())        missingParams.push_back("game_map");
        if (gameManagersFolder.empty()) missingParams.push_back("game_managers_folder");
        if (algorithm1File.empty())     missingParams.push_back("algorithm1");
        if (algorithm2File.empty())     missingParams.push_back("algorithm2");
    }
    if (modeCompetition) {
        if (gameMapsFolder.empty())     missingParams.push_back("game_maps_folder");
        if (gameManagerFile.empty())    missingParams.push_back("game_manager");
        if (algorithmsFolder.empty())   missingParams.push_back("algorithms_folder");
    }

    // If any parsing errors (unsupported or missing), print usage and error details, then exit:contentReference[oaicite:18]{index=18}:contentReference[oaicite:19]{index=19}
    if (!unknownArgs.empty() || !missingParams.empty()) {
        std::ostringstream usage;
        usage << "Usage:\n"
              << "  " << "simulator -comparative game_map=<file> game_managers_folder=<folder> algorithm1=<file> algorithm2=<file> [num_threads=<num>] [-verbose]\n"
              << "  " << "simulator -competition game_maps_folder=<folder> game_manager=<file> algorithms_folder=<folder> [num_threads=<num>] [-verbose]\n";
        if (!unknownArgs.empty()) {
            std::cerr << "Error: Unsupported command line arguments:";
            for (const std::string& ua : unknownArgs) {
                std::cerr << " " << ua;
            }
            std::cerr << "\n";
        }
        if (!missingParams.empty()) {
            std::cerr << "Error: Missing required arguments:";
            for (size_t j = 0; j < missingParams.size(); ++j) {
                std::cerr << " " << missingParams[j];
                if (j < missingParams.size() - 1) std::cerr << ",";
            }
            std::cerr << "\n";
        }
        std::cerr << usage.str();
        return 1;
    }

    // Validate numeric threads value (non-negative and meaningful)
    if (numThreads < 1) {
        // Treat numThreads <= 0 as invalid
        std::cerr << "Error: num_threads must be >= 1\n";
        std::cerr << "Usage: [see above]\n";
        return 1;
    }
    // We implement only single-thread mode. If num_threads > 1, we will still run sequentially.
    if (numThreads > 1) {
        // Informative message (not necessarily required, we could also ignore silently)
        std::cerr << "Note: Multi-threaded execution not implemented, running in single-thread mode.\n";
    }

    // Verify that files/folders exist and can be opened/traversed:contentReference[oaicite:20]{index=20}:contentReference[oaicite:21]{index=21}.
    // Also, ensure folders contain at least one relevant file as required.
    if (modeComparative) {
        // Map file
        if (!fs::exists(gameMapFile) || fs::is_directory(gameMapFile)) {
            std::cerr << "Error: game_map file '" << gameMapFile << "' does not exist or cannot be opened.\n";
            std::cerr << "Usage: [see above]\n";
            return 1;
        }
        // Game managers folder
        if (!fs::exists(gameManagersFolder) || !fs::is_directory(gameManagersFolder)) {
            std::cerr << "Error: game_managers_folder '" << gameManagersFolder << "' does not exist or cannot be opened.\n";
            std::cerr << "Usage: [see above]\n";
            return 1;
        }
        // Algorithms (algorithm1 & algorithm2)
        if (!fs::exists(algorithm1File) || fs::is_directory(algorithm1File)) {
            std::cerr << "Error: algorithm1 file '" << algorithm1File << "' does not exist or cannot be opened.\n";
            std::cerr << "Usage: [see above]\n";
            return 1;
        }
        if (!fs::exists(algorithm2File) || fs::is_directory(algorithm2File)) {
            std::cerr << "Error: algorithm2 file '" << algorithm2File << "' does not exist or cannot be opened.\n";
            std::cerr << "Usage: [see above]\n";
            return 1;
        }
    }
    if (modeCompetition) {
        // Game maps folder
        if (!fs::exists(gameMapsFolder) || !fs::is_directory(gameMapsFolder)) {
            std::cerr << "Error: game_maps_folder '" << gameMapsFolder << "' does not exist or cannot be opened.\n";
            std::cerr << "Usage: [see above]\n";
            return 1;
        }
        // Check that the folder has at least one file (valid map) inside
        bool hasMapFile = false;
        for (auto& entry : fs::directory_iterator(gameMapsFolder)) {
            if (entry.is_regular_file()) { hasMapFile = true; break; }
        }
        if (!hasMapFile) {
            std::cerr << "Error: game_maps_folder '" << gameMapsFolder << "' contains no map files.\n";
            std::cerr << "Usage: [see above]\n";
            return 1;
        }
        // Game manager .so file
        if (!fs::exists(gameManagerFile) || fs::is_directory(gameManagerFile)) {
            std::cerr << "Error: game_manager file '" << gameManagerFile << "' does not exist or cannot be opened.\n";
            std::cerr << "Usage: [see above]\n";
            return 1;
        }
        // Algorithms folder
        if (!fs::exists(algorithmsFolder) || !fs::is_directory(algorithmsFolder)) {
            std::cerr << "Error: algorithms_folder '" << algorithmsFolder << "' does not exist or cannot be opened.\n";
            std::cerr << "Usage: [see above]\n";
            return 1;
        }
        bool hasAlgFile = false;
        for (auto& entry : fs::directory_iterator(algorithmsFolder)) {
            if (entry.is_regular_file()) { hasAlgFile = true; break; }
        }
        if (!hasAlgFile) {
            std::cerr << "Error: algorithms_folder '" << algorithmsFolder << "' contains no algorithm files.\n";
            std::cerr << "Usage: [see above]\n";
            return 1;
        }
    }

    // Special check for competition: need at least 2 algorithms in folder:contentReference[oaicite:22]{index=22}.
    if (modeCompetition) {
        // Count .so files in algorithms_folder
        size_t algCount = 0;
        for (auto& entry : fs::directory_iterator(algorithmsFolder)) {
            if (entry.is_regular_file()) {
                std::string fname = entry.path().filename().string();
                // Consider only files with ".so" extension as algorithm libraries
                if (entry.path().extension() == ".so") {
                    ++algCount;
                }
            }
        }
        if (algCount < 2) {
            std::cerr << "Error: algorithms_folder '" << algorithmsFolder << "' must contain at least 2 algorithm .so files.\n";
            std::cerr << "Usage: [see above]\n";
            return 1;
        }
    }

    // At this point, basic input validation is done. Proceed to load files and run simulations.

    // Read and parse the game map (for comparative mode only, one map):contentReference[oaicite:23]{index=23}:contentReference[oaicite:24]{index=24}.
    size_t mapRows = 0, mapCols = 0;
    size_t maxSteps = 0, numShells = 0;
    std::string mapName;
    std::vector<std::string> mapGrid; // store the map lines

    if (modeComparative) {
        std::ifstream mapFile(gameMapFile);
        if (!mapFile) {
            std::cerr << "Error: Failed to open map file '" << gameMapFile << "' for reading.\n";
            return 1;
        }
        std::string line;
        // Line 1: map name/description
        if (!std::getline(mapFile, line)) {
            std::cerr << "Error: Map file is empty or invalid format (missing lines).\n";
            return 1;
        }
        mapName = line;
        // Line 2: MaxSteps = <NUM>
        if (!std::getline(mapFile, line)) {
            std::cerr << "Error: Map file missing MaxSteps.\n";
            return 1;
        }
        line = trim(line);
        if (line.rfind("MaxSteps", 0) != std::string::npos) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                try {
                    maxSteps = std::stoul(trim(line.substr(eq+1)));
                } catch (...) { maxSteps = 0; }
            }
        }
        // Line 3: NumShells = <NUM>
        if (!std::getline(mapFile, line)) {
            std::cerr << "Error: Map file missing NumShells.\n";
            return 1;
        }
        line = trim(line);
        if (line.rfind("NumShells", 0) != std::string::npos) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                try {
                    numShells = std::stoul(trim(line.substr(eq+1)));
                } catch (...) { numShells = 0; }
            }
        }
        // Line 4: Rows = <NUM>
        if (!std::getline(mapFile, line)) {
            std::cerr << "Error: Map file missing Rows.\n";
            return 1;
        }
        line = trim(line);
        if (line.rfind("Rows", 0) != std::string::npos) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                try {
                    mapRows = std::stoul(trim(line.substr(eq+1)));
                } catch (...) { mapRows = 0; }
            }
        }
        // Line 5: Cols = <NUM>
        if (!std::getline(mapFile, line)) {
            std::cerr << "Error: Map file missing Cols.\n";
            return 1;
        }
        line = trim(line);
        if (line.rfind("Cols", 0) != std::string::npos) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                try {
                    mapCols = std::stoul(trim(line.substr(eq+1)));
                } catch (...) { mapCols = 0; }
            }
        }
        if (mapRows == 0 || mapCols == 0) {
            std::cerr << "Error: Map file has invalid Rows/Cols values.\n";
            return 1;
        }
        // Read the map grid lines
        mapGrid.reserve(mapRows);
        size_t rowCount = 0;
        while (rowCount < mapRows && std::getline(mapFile, line)) {
            // If line is shorter than Cols, pad with spaces; if longer, truncate.
            if (line.size() < mapCols) {
                line.resize(mapCols, ' ');
            } else if (line.size() > mapCols) {
                line = line.substr(0, mapCols);
            }
            mapGrid.push_back(line);
            ++rowCount;
        }
        // If file had fewer lines than Rows, append empty lines
        while (mapGrid.size() < mapRows) {
            mapGrid.emplace_back(mapCols, ' ');
        }
        // We can ignore any extra lines beyond mapRows (per spec they are ignored).
    }

    // Dynamic loading of .so files:
    // Load Algorithm libraries first (so that Player/TankAlgorithm classes are registered),
    // then GameManager libraries. The Simulator uses automatic registration: when dlopen 
    // loads a library, the global static registration objects inside it will call our 
    // Simulator::register* functions to add factories:contentReference[oaicite:25]{index=25}.

    // Load algorithms in comparative mode (two specific files) or competition mode (all in folder).
    if (modeComparative) {
        // Possibly the two algorithm files could be the same (allowed):contentReference[oaicite:26]{index=26}.
        bool sameFile = (fs::absolute(algorithm1File) == fs::absolute(algorithm2File));
        // Load algorithm1 .so
        
        const std::string alg1Abs = fs::absolute(algorithm1File).string();///////////////////////////////////////////
        AlgorithmRegistrar::get().createAlgorithmFactoryEntry(
        fs::path(alg1Abs).filename().string());
        void* handle1 = dlopen(alg1Abs.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if (!handle1) {
            std::cerr << "dlopen failed for " << alg1Abs << ": " << dlerror() << "\n";
            AlgorithmRegistrar::get().removeLast();
            return 1;
        }

        try {
            AlgorithmRegistrar::get().validateLastRegistration();
        } catch (AlgorithmRegistrar::BadRegistrationException& e) {
            std::cerr << "Error: Algorithm file '" 
              << fs::path(alg1Abs).filename().string()
              << "' did not register required classes.\n"
              << "  name set? " << std::boolalpha << e.hasName << "\n"
              << "  player factory? " << e.hasPlayerFactory << "\n"
              << "  tank factory? "   << e.hasTankAlgorithmFactory << "\n";
            AlgorithmRegistrar::get().removeLast();
            dlclose(handle1);
            return 1;
        }
        if (!handle1) {
            std::cerr << "Error: Failed to load algorithm1 library (" << algorithm1File << "): " << dlerror() << "\n";
            return 1;
        }
        loadedHandles.push_back(handle1);
        // Validate that exactly one Player and one TankAlgorithm were registered
        if (playerFactories.size() < 1 || tankFactories.size() < 1) {
            std::cerr << "Error: Algorithm file '" << algorithm1File << "' did not register required classes.\n";
            return 1;
        }
        size_t alg1PlayerIndex = playerFactories.size() - 1;
        size_t alg1TankIndex = tankFactories.size() - 1;
        // Save algorithm1 entry
        AlgorithmEntry alg1;
        alg1.name = fs::path(algorithm1File).filename().string();
        alg1.playerFactory = playerFactories[alg1PlayerIndex];
        alg1.tankFactory = tankFactories[alg1TankIndex];
        alg1.score = 0;
        algorithms.push_back(alg1);

        // Load algorithm2 if it's a different file
        //size_t alg2Index = 0;
        
        if (!sameFile) {/////////////////////////////////////////////////////////////////////////////////
            const std::string alg2Abs = fs::absolute(algorithm2File).string();
            AlgorithmRegistrar::get().createAlgorithmFactoryEntry(
            fs::path(alg2Abs).filename().string());
            void* handle2 = dlopen(alg2Abs.c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!handle2) {
                std::cerr << "dlopen failed for " << alg2Abs << ": " << dlerror() << "\n";
                // cleanup the pending (empty) entry
                AlgorithmRegistrar::get().removeLast();
                return 1;
            }
            try {
                AlgorithmRegistrar::get().validateLastRegistration();
            } catch (AlgorithmRegistrar::BadRegistrationException& e) {
                std::cerr << "Error: Algorithm file '" 
                  << fs::path(alg1Abs).filename().string()
                  << "' did not register required classes.\n"
                  << "  name set? " << std::boolalpha << e.hasName << "\n"
                  << "  player factory? " << e.hasPlayerFactory << "\n"
                  << "  tank factory? "   << e.hasTankAlgorithmFactory << "\n";
                AlgorithmRegistrar::get().removeLast();
                dlclose(handle1);
                return 1;
            }
            if (!handle2) {
                std::cerr << "Error: Failed to load algorithm2 library (" << algorithm2File << "): " << dlerror() << "\n";
                return 1;
            }
            loadedHandles.push_back(handle2);
            if (playerFactories.size() < 2 || tankFactories.size() < 2) {
                std::cerr << "Error: Algorithm file '" << algorithm2File << "' did not register required classes.\n";
                return 1;
            }
            size_t alg2PlayerIndex = playerFactories.size() - 1;
            size_t alg2TankIndex = tankFactories.size() - 1;
            AlgorithmEntry alg2;
            alg2.name = fs::path(algorithm2File).filename().string();
            alg2.playerFactory = playerFactories[alg2PlayerIndex];
            alg2.tankFactory = tankFactories[alg2TankIndex];
            alg2.score = 0;
            algorithms.push_back(alg2);
            //alg2Index = 1;
        } else {
            // If same file used for both algorithm1 and algorithm2:
            //alg2Index = 0;
        }

        

        // Load all GameManager libraries from the folder
        /*for (auto& entry : fs::directory_iterator(gameManagersFolder)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".so") continue;
            std::string gmPath = entry.path().string();
            // Open the .so file
            const std::string gmPathAbs = fs::absolute(entry.path()).string();
            void* gmHandle = dlopen(gmPathAbs.c_str(), RTLD_NOW);
            if (!gmHandle) {
                std::cerr << "Error: Failed to load GameManager library (" << gmPath << "): " << dlerror() << "\n";
                // Continue to next or abort? We choose to abort on any failure.
                return 1;
            }
            loadedHandles.push_back(gmHandle);
            // Validate registration of exactly one GameManager class
            if (gmFactories.size() < gmNames.size() + 1) {
                // gmNames is updated only after successful registration
                gmNames.push_back(entry.path().filename().string());
                if (gmFactories.size() < gmNames.size()) {
                    std::cerr << "Error: GameManager file '" << gmPath << "' did not register a GameManager class.\n";
                    return 1;
                }
            } else {
                gmNames.push_back(entry.path().filename().string());
            }
        }
        */

        for (auto& entry : fs::directory_iterator(gameManagersFolder)) {
    if (!entry.is_regular_file()) continue;
    if (entry.path().extension() != ".so") continue;

    const std::string gmPathAbs  = fs::absolute(entry.path()).string();
    const std::string gmFileName = entry.path().filename().string();

    // --- Create registrar entry BEFORE dlopen (API name is createGameManagerEntry)
    auto& gmReg = GameManagerRegistrar::get();
    gmReg.createGameManagerEntry(gmFileName);

    // Snapshot sizes so we can roll back safely on failure
    const std::size_t gmFactoriesBefore = gmFactories.size();
    const std::size_t gmRegCountBefore  = gmReg.count();

    // --- dlopen
    void* gmHandle = dlopen(gmPathAbs.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!gmHandle) {
        std::cerr << "Error: Failed to load GameManager library (" << gmFileName
                  << "): " << dlerror() << "\n";
        // Drop the just-created registrar entry
        if (gmReg.count() > gmRegCountBefore) gmReg.removeLast();
        return 1; // abort like your algo path
    }

    // --- Validate the last GM registration
    try {
        gmReg.validateLastRegistration();
    } catch (GameManagerRegistrar::BadRegistrationException& e) {
        std::cerr << "Error: GameManager file '" << gmFileName
                  << "' did not register a GameManager class.\n";
        // Roll back anything pushed before unloading the .so
        while (gmFactories.size() > gmFactoriesBefore) gmFactories.pop_back();
        if (gmReg.count() > gmRegCountBefore) gmReg.removeLast();
        dlclose(gmHandle);
        return 1;
    }

    // Optional: enforce exactly one new GM factory
    if (gmFactories.size() != gmFactoriesBefore + 1) {
        std::cerr << "Error: GameManager file '" << gmFileName
                  << "' did not register exactly one GameManager class.\n";
        while (gmFactories.size() > gmFactoriesBefore) gmFactories.pop_back();
        if (gmReg.count() > gmRegCountBefore) gmReg.removeLast();
        dlclose(gmHandle);
        return 1;
    }

    // Success: record name and keep the handle
    gmNames.push_back(gmFileName);
    loadedHandles.push_back(gmHandle);
}
   

        if (gmNames.empty()) {
            std::cerr << "Error: No GameManager .so files found in folder '" << gameManagersFolder << "'.\n";
            return 1;
        }

        // Run each game manager with the two algorithms on the single map
        struct OutcomeKey {
            int winner;
            GameResult::Reason reason;
            size_t rounds;
            std::vector<std::string> finalMap; // final board state as lines
            std::vector<size_t> remaining_tanks;
        };
        struct OutcomeGroup {
            OutcomeKey key;
            std::vector<std::string> gmList;
        };
        std::vector<OutcomeGroup> outcomeGroups;
        outcomeGroups.reserve(gmNames.size());

        for (size_t gi = 0; gi < gmNames.size(); ++gi) {
            // Create a new GameManager instance
            std::unique_ptr<AbstractGameManager> gm = gmFactories[gi](verboseFlag);
            if (!gm) {
                std::cerr << "Error: Failed to create GameManager instance for " << gmNames[gi] << ".\n";
                continue; // skip this game if cannot instantiate (should not happen in correct submissions)
            }
            // Create two Player instances (one for each side)
            // player_index 1 and 2, with initial coords (0,0) and same maxSteps/numShells.
            std::unique_ptr<Player> player1 = algorithms[0].playerFactory(1, 0, 0, maxSteps, numShells);
            std::unique_ptr<Player> player2 = algorithms[ (algorithms.size() > 1 ? 1 : 0) ].playerFactory(2, 0, 0, maxSteps, numShells);
            if (!player1 || !player2) {
                std::cerr << "Error: Failed to create Player instances for algorithms.\n";
                return 1;
            }
            // Prepare a SatelliteView of the initial map. We implement a simple concrete SatelliteView for the snapshot.
            class InitialMapView : public SatelliteView {
                const std::vector<std::string>& grid;
            public:
                InitialMapView(const std::vector<std::string>& g) : grid(g) {}
                char getObjectAt(size_t x, size_t y) const override {
                    if (y < grid.size() && x < grid[y].size()) return grid[y][x];
                    return ' ';
                }
            } initialView(mapGrid);
            // Run the game
            GameResult result = gm->run(mapCols, mapRows, initialView, mapName, maxSteps, numShells,
                                        *player1, algorithms[0].name, *player2, algorithms[(algorithms.size()>1?1:0)].name,
                                        algorithms[0].tankFactory, algorithms[(algorithms.size()>1?1:0)].tankFactory);
            // Extract outcome
            OutcomeKey key;
            key.winner = result.winner;
            key.reason = result.reason;
            key.rounds = result.rounds;
            // Capture final map state from gameState
            key.finalMap.reserve(mapRows);
            key.remaining_tanks = result.remaining_tanks;
            if (result.gameState) {
                for (size_t r = 0; r < mapRows; ++r) {
                    std::string line;
                    line.reserve(mapCols);
                    for (size_t c = 0; c < mapCols; ++c) {
                        char ch = result.gameState->getObjectAt(c, r);
                        line.push_back(ch);
                    }
                    key.finalMap.push_back(line);
                }
            }
            // Destroy the gameState (to free memory from .so before unloading)
            result.gameState.reset();
            // Group the outcome with others that are identical
            bool grouped = false;
            for (auto& grp : outcomeGroups) {
                if (grp.key.winner == key.winner && grp.key.reason == key.reason && grp.key.rounds == key.rounds && grp.key.finalMap == key.finalMap) {
                    // Same outcome, add this game manager name to the group
                    grp.gmList.push_back(gmNames[gi]);
                    grouped = true;
                    break;
                }
            }
            if (!grouped) {
                OutcomeGroup newGroup;
                newGroup.key = std::move(key);
                newGroup.gmList.push_back(gmNames[gi]);
                outcomeGroups.push_back(std::move(newGroup));
            }
        }
        // Sort groups by descending size (so largest group of identical outcomes first):contentReference[oaicite:27]{index=27}.
        std::sort(outcomeGroups.begin(), outcomeGroups.end(), [](const OutcomeGroup& a, const OutcomeGroup& b){
            return a.gmList.size() > b.gmList.size();
        });

        auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        std::string outFileName = "comparative_results_" + std::to_string(timestamp) + ".txt";
        fs::path outPath = fs::current_path() / outFileName;  // <- CWD, not inside gameManagersFolder

        std::ofstream outFile(outPath);
        bool writeToScreen = false;
        if (!outFile) {
            std::cerr << "Error: Cannot create output file at '" << outPath.string() << "'. Printing results to screen.\n";
            writeToScreen = true;
        }
        std::ostream& out = writeToScreen ? std::cout : outFile;
        // Write header lines
        out << "game_map=" << fs::path(gameMapFile).filename().string() << "\n";
        out << "algorithm1=" << fs::path(algorithm1File).filename().string() << "\n";
        out << "algorithm2=" << fs::path(algorithm2File).filename().string() << "\n";
        out << "\n"; // blank line

        // Write each outcome group
        for (size_t g = 0; g < outcomeGroups.size(); ++g) {
            // Comma-separated list of game manager names for this group
            for (size_t k = 0; k < outcomeGroups[g].gmList.size(); ++k) {
                out << outcomeGroups[g].gmList[k];
                if (k < outcomeGroups[g].gmList.size() - 1) out << ",";
            }
            out << "\n";
            // Game result message (who won or tie and why):contentReference[oaicite:28]{index=28}
            int winner = outcomeGroups[g].key.winner;
            GameResult::Reason reason = outcomeGroups[g].key.reason;
            const std::vector<size_t>& remaining = outcomeGroups[g].key.remaining_tanks; // (Note: assume GameResult stores counts)
            // Construct result message similar to Assignment 2 output:
            if (winner == 0) {
                // Tie scenarios
                if (reason == GameResult::Reason::ALL_TANKS_DEAD) {
                    out << "Tie, both players have zero tanks\n";
                } else if (reason == GameResult::Reason::MAX_STEPS) {
                    out << "Tie, reached max steps = " << maxSteps 
                        << ", player 1 has " << (remaining.size() > 0 ? remaining[0] : 0) << " tanks, "
                        << "player 2 has " << (remaining.size() > 1 ? remaining[1] : 0) << " tanks\n";
                } else if (reason == GameResult::Reason::ZERO_SHELLS) {
                    out << "Tie, both players have zero shells for 40 steps\n";
                } else {
                    out << "Tie\n";
                }
            } else {
                // Winner scenarios
                int loser = (winner == 1 ? 2 : 1);
                //size_t winnerIndex = (winner - 1);
                out << "Player " << winner << " won: ";
                if (reason == GameResult::Reason::ALL_TANKS_DEAD) {
                    out << "all tanks of player " << loser << " are dead\n";
                } else if (reason == GameResult::Reason::MAX_STEPS) {
                    out << "maximum steps reached\n";
                } else if (reason == GameResult::Reason::ZERO_SHELLS) {
                    out << "all shells are gone\n";
                } else {
                    out << "unknown reason\n";
                }
            }
            // Round number of game end
            out << outcomeGroups[g].key.rounds << "\n";
            // Final map state lines
            for (const std::string& line : outcomeGroups[g].key.finalMap) {
                out << line << "\n";
            }
            // Add an empty line between groups (if more groups remain):contentReference[oaicite:29]{index=29}
            if (g < outcomeGroups.size() - 1) {
                out << "\n";
            }
        }

    } else if (modeCompetition) {
        // Load all algorithm .so files from algorithmsFolder
        std::vector<std::string> algFiles;
        for (auto& entry : fs::directory_iterator(algorithmsFolder)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".so") continue;
            algFiles.push_back(entry.path().string());
        }
        // Optionally sort algorithm files by name to have consistent indexing (not strictly required, but for deterministic order)
        std::sort(algFiles.begin(), algFiles.end());
        // Load each algorithm library
        for (const std::string& algPath : algFiles) {
           const std::string algAbs = fs::absolute(algPath).string();
            AlgorithmRegistrar::get().createAlgorithmFactoryEntry(fs::path(algAbs).filename().string());
            void* handle = dlopen(algAbs.c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!handle) {
                std::cerr << "Error: Failed to load algorithm library (" << algPath << "): " << dlerror() << "\n";
                AlgorithmRegistrar::get().removeLast();
                return 1;
            }
            AlgorithmRegistrar::get().validateLastRegistration();
            if (!handle) {
                std::cerr << "Error: Failed to load algorithm library (" << algPath << "): " << dlerror() << "\n";
                return 1;
            }
            loadedHandles.push_back(handle);
            // Validate registration: one Player and one TankAlgorithm added
            if (playerFactories.size() < algorithms.size() + 1 || tankFactories.size() < algorithms.size() + 1) {
                std::cerr << "Error: Algorithm file '" << algPath << "' did not register required classes.\n";
                return 1;
            }
            // Get the newly registered factories (last in vectors)
            size_t pIndex = playerFactories.size() - 1;
            size_t tIndex = tankFactories.size() - 1;
            AlgorithmEntry alg;
            alg.name = fs::path(algPath).filename().string();
            alg.playerFactory = playerFactories[pIndex];
            alg.tankFactory = tankFactories[tIndex];
            alg.score = 0;
            algorithms.push_back(alg);
        }
        size_t N = algorithms.size();
        // Ensure at least 2 algorithms (we checked earlier, so this should hold)
        if (N < 2) {
            std::cerr << "Error: Not enough algorithms to run competition.\n";
            return 1;
        }
        // Load the single GameManager .so file
        const std::string gmAbs = fs::absolute(gameManagerFile).string();
        void* gmHandle = dlopen(gmAbs.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if (!gmHandle) {
            std::cerr << "Error: Failed to load GameManager library (" << gameManagerFile << "): " << dlerror() << "\n";
            return 1;
        }
        loadedHandles.push_back(gmHandle);
        // Validate one GameManager registered
        if (gmFactories.empty()) {
            std::cerr << "Error: GameManager file '" << gameManagerFile << "' did not register a GameManager class.\n";
            return 1;
        }
        gmNames.push_back(fs::path(gameManagerFile).filename().string());

        // List all map files in gameMapsFolder
        std::vector<std::string> mapFiles;
        for (auto& entry : fs::directory_iterator(gameMapsFolder)) {
            if (!entry.is_regular_file()) continue;
            // Consider any regular file as a map file (assuming correct format)
            mapFiles.push_back(entry.path().string());
        }
        std::sort(mapFiles.begin(), mapFiles.end());
        if (mapFiles.empty()) {
            std::cerr << "Error: No map files found in folder '" << gameMapsFolder << "'.\n";
            return 1;
        }

        // Create one GameManager instance (per game run, we'll create a fresh instance for safety)
        // Use index 0 factory since only one GM type is loaded.
        GameManagerFactory gmFactory = gmFactories[0];

        // Run competition games according to schedule:contentReference[oaicite:30]{index=30}:contentReference[oaicite:31]{index=31}.
        size_t K = mapFiles.size();
        for (size_t k = 0; k < K; ++k) {
            // Parse the map file k (similar to comparative map parsing above, for each map)
            std::ifstream mapFile(mapFiles[k]);
            if (!mapFile) {
                std::cerr << "Warning: Skipping unreadable map file '" << mapFiles[k] << "'.\n";
                continue;
            }
            std::string line;
            std::string mapNameK;
            size_t maxStepsK = 0, numShellsK = 0;
            size_t rowsK = 0, colsK = 0;
            std::vector<std::string> gridK;
            // Read map header lines (1-5)
            if (std::getline(mapFile, line)) mapNameK = line;
            if (std::getline(mapFile, line)) {
                line = trim(line);
                size_t eq = line.find('=');
                if (eq != std::string::npos) {
                    try { maxStepsK = std::stoul(trim(line.substr(eq+1))); } catch (...) {}
                }
            }
            if (std::getline(mapFile, line)) {
                line = trim(line);
                size_t eq = line.find('=');
                if (eq != std::string::npos) {
                    try { numShellsK = std::stoul(trim(line.substr(eq+1))); } catch (...) {}
                }
            }
            if (std::getline(mapFile, line)) {
                line = trim(line);
                size_t eq = line.find('=');
                if (eq != std::string::npos) {
                    try { rowsK = std::stoul(trim(line.substr(eq+1))); } catch (...) {}
                }
            }
            if (std::getline(mapFile, line)) {
                line = trim(line);
                size_t eq = line.find('=');
                if (eq != std::string::npos) {
                    try { colsK = std::stoul(trim(line.substr(eq+1))); } catch (...) {}
                }
            }
            if (rowsK == 0 || colsK == 0) {
                // Invalid map format, skip this map
                continue;
            }
            // Read grid lines
            gridK.reserve(rowsK);
            size_t rcount = 0;
            while (rcount < rowsK && std::getline(mapFile, line)) {
                if (line.size() < colsK) line.resize(colsK, ' ');
                else if (line.size() > colsK) line = line.substr(0, colsK);
                gridK.push_back(line);
                ++rcount;
            }
            while (gridK.size() < rowsK) {
                gridK.emplace_back(colsK, ' ');
            }

            // For each algorithm index i, determine its opponent j for map k and run one game (avoid duplicate pairs when needed)
            for (size_t i = 0; i < N; ++i) {
                size_t offset = (N > 1 ? k % (N - 1) : 0);
                size_t j = (i + 1 + offset) % N;
                if (i >= j) {
                    // Ensure each pair is run only once (skip duplicates, especially when offset = N/2 - 1 for even N):contentReference[oaicite:32]{index=32}
                    continue;
                }
                // Instantiate GameManager for this game
                std::unique_ptr<AbstractGameManager> gm = gmFactory(verboseFlag);
                if (!gm) {
                    std::cerr << "Error: Could not create GameManager instance for competition.\n";
                    continue;
                }
                // Instantiate players for algorithm i and j
                std::unique_ptr<Player> playerA = algorithms[i].playerFactory(1, 0, 0, maxStepsK, numShellsK);
                std::unique_ptr<Player> playerB = algorithms[j].playerFactory(2, 0, 0, maxStepsK, numShellsK);
                if (!playerA || !playerB) {
                    std::cerr << "Error: Could not create Player instances for algorithms " << algorithms[i].name << " and " << algorithms[j].name << ".\n";
                    continue;
                }
                // Create SatelliteView for this map
                class MapView : public SatelliteView {
                    const std::vector<std::string>& grid;
                public:
                    MapView(const std::vector<std::string>& g) : grid(g) {}
                    char getObjectAt(size_t x, size_t y) const override {
                        if (y < grid.size() && x < grid[y].size()) return grid[y][x];
                        return ' ';
                    }
                } satelliteViewK(gridK);
                // Run the game: algorithm i is player1, algorithm j is player2
                GameResult result = gm->run(colsK, rowsK, satelliteViewK, mapNameK, maxStepsK, numShellsK,
                                            *playerA, algorithms[i].name, *playerB, algorithms[j].name,
                                            algorithms[i].tankFactory, algorithms[j].tankFactory);
                // Update scores:contentReference[oaicite:33]{index=33}
                if (result.winner == 1) {
                    algorithms[i].score += 3;
                } else if (result.winner == 2) {
                    algorithms[j].score += 3;
                } else if (result.winner == 0) {
                    // tie
                    algorithms[i].score += 1;
                    algorithms[j].score += 1;
                }
                // Clean up gameState if exists and any leftover (we don't need final map here)
                result.gameState.reset();
            }
        }

        // Prepare output file "competition_<time>.txt" in algorithms_folder
        auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        std::string outFileName = "comparative_results_" + std::to_string(timestamp) + ".txt";
        fs::path outPath = fs::current_path() / outFileName;
        std::ofstream outFile(outPath);
        bool writeToScreen = false;
        if (!outFile) {
            std::cerr << "Error: Cannot create output file at '" << outPath.string() << "'. Printing results to screen.\n";
            writeToScreen = true;
        }
        std::ostream& out = writeToScreen ? std::cout : outFile;

        // Header lines
        out << "game_maps_folder=" << fs::path(gameMapsFolder).filename().string() << "\n";
        out << "game_manager=" << fs::path(gameManagerFile).filename().string() << "\n";
        out << "\n";
        // Sort algorithms by score (desc):contentReference[oaicite:34]{index=34}
        std::sort(algorithms.begin(), algorithms.end(), [](const AlgorithmEntry& a, const AlgorithmEntry& b){
            return a.score > b.score;
        });
        // Output each algorithm name and total score:contentReference[oaicite:35]{index=35}
        for (const AlgorithmEntry& alg : algorithms) {
            out << alg.name << " " << alg.score << "\n";
        }
    }

    // Clean up: destroy all loaded libraries (after all created objects are out of scope):contentReference[oaicite:36]{index=36}.
   // Ensure all plugin-owned objects/factories are destroyed BEFORE dlclose
algorithms.clear();        // destroys AlgorithmEntry copies of factories
gmFactories.clear();       // destroys GameManager factories
playerFactories.clear();   // destroys Player factories
tankFactories.clear();     // destroys TankAlgorithm factories
gmNames.clear();           // optional

// Also clear registrars that may still hold std::function from the .so
AlgorithmRegistrar::get().clear();
GameManagerRegistrar::get().clear();

// Now it's safe to unload the shared objects.
// Do it in reverse order and skip duplicate handles.
{
    std::unordered_set<void*> closed;
    for (auto it = loadedHandles.rbegin(); it != loadedHandles.rend(); ++it) {
        void* h = *it;
        if (!h || closed.count(h)) continue;
        closed.insert(h);

        if (dlclose(h) != 0) {
            const char* err = dlerror();
            if (err) std::cerr << "dlclose error: " << err << "\n";
        }
    }
    loadedHandles.clear();
}

    return 0;
}

// A minimal main function to invoke the Simulator (could be placed in a separate file in practice).
int main(int argc, char* argv[]) {
    return Simulator::getInstance().run(argc, argv);
}
