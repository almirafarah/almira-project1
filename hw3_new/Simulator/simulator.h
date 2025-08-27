#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "../common/Player.h"
#include "../common/TankAlgorithm.h"
#include "../common/GameResult.h"
#include "../common/AbstractGameManager.h"
#include "../common/GameManagerRegistration.h"
#include "../common/PlayerRegistration.h"
#include "../common/TankAlgorithmRegistration.h"

// The Simulator is implemented as a Singleton:contentReference[oaicite:15]{index=15} that manages dynamic loading 
// of GameManager and Algorithm classes and runs games in comparative or competition modes.
class Simulator {
public:
    // Retrieve the singleton instance of Simulator.
    static Simulator& getInstance();

    // Parse command-line arguments and run the simulation.
    // Returns 0 on success, non-zero on error (usage or runtime error).
    int run(int argc, char* argv[]);

    // The following static methods are called by the auto-registration 
    // structs when .so files are loaded (via REGISTER_PLAYER, etc.). They add 
    // the provided factory to the Simulator's registry:contentReference[oaicite:16]{index=16}.
    static void registerGameManagerFactory(GameManagerFactory factory);
    static void registerPlayerFactory(PlayerFactory factory);
    static void registerTankAlgorithmFactory(TankAlgorithmFactory factory);

private:
    // Private constructor for Singleton pattern.
    Simulator() = default;
    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;

    // Internal structure to hold an Algorithm's factories and name.
    struct AlgorithmEntry {
        std::string name;
        PlayerFactory playerFactory;
        TankAlgorithmFactory tankFactory;
        int score; // used in competition mode
    };

    // Registered factories (populated via static registration when .so are loaded)
    std::vector<GameManagerFactory> gmFactories;
    std::vector<PlayerFactory> playerFactories;
    std::vector<TankAlgorithmFactory> tankFactories;

    // Lists of entities for simulation
    std::vector<std::string> gmNames;               // names of loaded GameManagers
    std::vector<AlgorithmEntry> algorithms;         // loaded algorithms (with name, factories, score)

    // Keep track of open dynamic library handles for later dlclose()
    std::vector<void*> loadedHandles;
};
