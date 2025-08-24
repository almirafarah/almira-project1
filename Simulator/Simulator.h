#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <string>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include "../common/SatelliteView.h"
#include "../common/GameResult.h"

// Platform-specific dynamic library loading
// No dynamic library loading needed

// Forward declarations
struct GameTask;
class AbstractGameManager;
class Player;
class TankAlgorithm;

// Wrapper for GameResult with additional metadata needed by Simulator
struct SimulatorGameResult {
    std::string game_manager_name;
    std::string algorithm1_name;
    std::string algorithm2_name;
    // For output compliance, keep the original filenames (with extensions)
    std::string game_manager_file;
    std::string algorithm1_file;
    std::string algorithm2_file;
    std::string map_name;
    std::string map_path;  // Full path to map file
    size_t map_width;
    size_t map_height;
    ::GameResult game_result;  // Use the common GameResult
    
    SimulatorGameResult() 
        : game_manager_name("")
        , algorithm1_name("")
        , algorithm2_name("")
        , game_manager_file("")
        , algorithm1_file("")
        , algorithm2_file("")
        , map_name("")
        , map_path("")
        , map_width(0)
        , map_height(0) {}
    
    // Default copy constructor and copy assignment operator (now that GameResult is copyable)
    SimulatorGameResult(const SimulatorGameResult&) = default;
    SimulatorGameResult& operator=(const SimulatorGameResult&) = default;
    
    // Move constructor and move assignment operator
    SimulatorGameResult(SimulatorGameResult&& other) noexcept = default;
    SimulatorGameResult& operator=(SimulatorGameResult&& other) noexcept = default;
};

// Game task structure for thread pool
struct GameTask {
    std::string game_manager_path;
    std::string algorithm1_path;
    std::string algorithm2_path;
    std::string map_path;
    std::string map_name;
    size_t map_width;
    size_t map_height;
    size_t max_steps;
    size_t num_shells;
    bool verbose;
    
    // Default constructor
    GameTask() 
        : max_steps(50)
        , num_shells(10)
        , map_width(0)
        , map_height(0)
        , verbose(false) {}
    
    GameTask(const std::string& gm, const std::string& alg1, const std::string& alg2, 
             const std::string& map, const std::string& map_name, 
             size_t width, size_t height, size_t steps, size_t shells, bool v)
        : game_manager_path(gm)
        , algorithm1_path(alg1)
        , algorithm2_path(alg2)
        , map_path(map)
        , map_name(map_name)
        , map_width(width)
        , map_height(height)
        , max_steps(steps)
        , num_shells(shells)
        , verbose(v) {}
};

class Simulator {
public:
    Simulator();
    ~Simulator();
    
    // Main entry points for the two modes
    bool runComparative(const std::string& game_map, const std::string& game_managers_folder,
                       const std::string& algorithm1, const std::string& algorithm2,
                       int num_threads = 1, bool verbose = false);
    
    bool runCompetition(const std::string& game_maps_folder, const std::string& game_manager,
                       const std::string& algorithms_folder, int num_threads = 1, bool verbose = false);

private:
    // Thread pool management
    void initializeThreadPool(int num_threads, size_t total_tasks);
    void workerThread();
    void submitTask(const GameTask& task);
    void waitForAllTasks();
    
    // Dynamic library loading
    bool loadAlgorithmLibrary(const std::string& library_path);
    bool loadGameManagerLibrary(const std::string& library_path);
    
    // Game execution
    // Note: Simulator creates Player objects as raw pointers (cannot assume copy constructors exist)
    // Passes references to GameManager, ownership stays with Simulator
    SimulatorGameResult executeGame(const GameTask& task);
    static std::unique_ptr<SatelliteView> createMapFromFile(const std::string& map_path, size_t width, size_t height);

    // Output generation
    static void writeComparativeResults(const std::string& output_folder,
                                        const std::vector<SimulatorGameResult>& results);
    static void writeCompetitionResults(const std::string& output_folder,
                                        const std::string& game_maps_folder,
                                        const std::string& game_manager_file,
                                        const std::vector<SimulatorGameResult>& results);

    // Utility functions
    static std::string getLibraryName(const std::string& path);
    static std::vector<std::string> getFilesInFolder(const std::string& folder, const std::string& extension);
    static std::string generateTimestamp();
    
    // Thread pool members
    std::vector<std::thread> workers_;
    std::queue<GameTask> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_workers_;
    
    // Results storage
    std::vector<SimulatorGameResult> game_results_;
    std::mutex results_mutex_;
    
    // Configuration
    bool verbose_;

    // Keep dynamic libraries alive for the duration of the run
    std::vector<std::unique_ptr<class DynamicLibraryLoader>> loaded_algorithm_libraries_;
    std::vector<std::unique_ptr<class DynamicLibraryLoader>> loaded_gamemanager_libraries_;
};

#endif // SIMULATOR_H
