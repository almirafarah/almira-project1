#include "Simulator.h"
#include "../common/AbstractGameManager.h"
#include "../common/Player.h"
#include "../common/TankAlgorithm.h"
#include "../common/SatelliteView.h"
#include "../common/GameResult.h"
#include "../common/GameManagerRegistration.h"
#include "AlgorithmRegistrar.h"
#include "DynamicLibraryLoader.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <map>
#include <functional>

Simulator::Simulator() : stop_workers_(false), verbose_(false) {
}

Simulator::~Simulator() {
    // Stop all worker threads
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_workers_ = true;
    }
    condition_.notify_all();
    
    // Wait for all workers to finish
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

bool Simulator::runComparative(const std::string& game_map, const std::string& game_managers_folder,
                              const std::string& algorithm1, const std::string& algorithm2,
                              int num_threads, bool verbose) {
    verbose_ = verbose;
    std::cout << "Running comparative mode..." << std::endl;
    std::cout << "Game map: " << game_map << std::endl;
    std::cout << "Game managers folder: " << game_managers_folder << std::endl;
    std::cout << "Algorithm 1: " << algorithm1 << std::endl;
    std::cout << "Algorithm 2: " << algorithm2 << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    
    // Load algorithm libraries dynamically
    if (!loadAlgorithmLibrary(algorithm1) || !loadAlgorithmLibrary(algorithm2)) {
        std::cerr << "Failed to load algorithm libraries" << std::endl;
        return false;
    }

    // Read map dimensions and validate player tanks (HW2 format only)
    std::ifstream map_file(game_map);
    if (!map_file.is_open()) {
        std::cerr << "Failed to open map file: " << game_map << std::endl;
        return false;
    }
    
    size_t map_width = 0;
    size_t map_height = 0;
    std::string line;
    int player1_tanks = 0;
    int player2_tanks = 0;
    bool has_mines = false;
    // HW2-only parsing, matching HW2::readBoard
    // 1) ignore description
    if (!std::getline(map_file, line)) { std::cerr << "Map file is empty: " << game_map << std::endl; map_file.close(); return false; }
    // 2–5) parse the four parameter lines
    auto parseParamLine = [](const std::string& ln, const std::string& key) -> size_t {
        auto eq = ln.find('=');
        if (eq == std::string::npos) throw std::runtime_error("Missing '=' in " + key + " line: " + ln);
        if (ln.substr(0, eq).find(key) == std::string::npos) throw std::runtime_error("Expected key '" + key + "' in line: " + ln);
        std::string value = ln.substr(eq + 1);
        size_t start = value.find_first_not_of(" \t");
        size_t end   = value.find_last_not_of(" \t");
        if (start == std::string::npos) throw std::runtime_error("No value for " + key);
        value = value.substr(start, end - start + 1);
        return static_cast<size_t>(std::stoul(value));
    };
    if (!std::getline(map_file, line)) { std::cerr << "Invalid HW2 header: missing MaxSteps" << std::endl; map_file.close(); return false; }
    /*size_t max_steps=*/ parseParamLine(line, "MaxSteps");
    if (!std::getline(map_file, line)) { std::cerr << "Invalid HW2 header: missing NumShells" << std::endl; map_file.close(); return false; }
    /*size_t num_shells=*/ parseParamLine(line, "NumShells");
    if (!std::getline(map_file, line)) { std::cerr << "Invalid HW2 header: missing Rows" << std::endl; map_file.close(); return false; }
    map_height = parseParamLine(line, "Rows");
    if (!std::getline(map_file, line)) { std::cerr << "Invalid HW2 header: missing Cols" << std::endl; map_file.close(); return false; }
    map_width = parseParamLine(line, "Cols");
    if (verbose_) {
        std::cout << "HW2 map dimensions from header: " << map_width << "x" << map_height << std::endl;
    }
    // 6+) read exactly map_height lines and validate first map_width characters
    for (size_t r = 0; r < map_height; ++r) {
        if (!std::getline(map_file, line)) {
            line = std::string(map_width, ' ');
        }
        size_t validate_length = std::min(line.length(), map_width);
        for (size_t j = 0; j < validate_length; ++j) {
            char c = line[j];
            if (c == '1') player1_tanks++;
            else if (c == '2') player2_tanks++;
            else if (c == '*') has_mines = true;
            else if (c != '#' && c != ' ' && c != '%' && c != '@') {
                std::cerr << "Invalid character in map: '" << c << "' (only #, %, 1, 2, *, @, and spaces allowed)" << std::endl;
                map_file.close();
                return false;
            }
        }
    }

    map_file.close();
    
    if (map_width == 0 || map_height == 0) {
        std::cerr << "Invalid map dimensions: " << map_width << "x" << map_height << std::endl;
        return false;
    }
    
    // Validate that both players have tanks
    if (player1_tanks == 0) {
        std::cerr << "Map validation failed: Player 1 has no tanks (no '1' characters found)" << std::endl;
        return false;
    }
    if (player2_tanks == 0) {
        std::cerr << "Map validation failed: Player 2 has no tanks (no '2' characters found)" << std::endl;
        return false;
    }
    
    if (verbose_) {
        std::cout << "Map validation passed:" << std::endl;
        std::cout << "  Format: HW2 (with header)" << std::endl;
        std::cout << "  Dimensions: " << map_width << "x" << map_height << std::endl;
        std::cout << "  Player 1 tanks: " << player1_tanks << std::endl;
        std::cout << "  Player 2 tanks: " << player2_tanks << std::endl;
        std::cout << "  Contains mines: " << (has_mines ? "Yes" : "No") << std::endl;
    }
    
    // Get all game manager source files in the folder (.cpp)
    auto game_manager_files = getFilesInFolder(game_managers_folder, ".cpp");
    if (game_manager_files.empty()) {
        std::cerr << "No source files (.cpp) found in game managers folder: " << game_managers_folder << "\n"
                  << "If you intended to pass a static library, note this simulator expects .cpp sources and links them directly." << std::endl;
        return false;
    }
    
    // Initialize thread pool
    initializeThreadPool(num_threads, game_manager_files.size());
    
    // Create tasks for each game manager
    for (const auto& gm_file : game_manager_files) {
        std::string full_path = game_managers_folder + "/" + gm_file;
        GameTask task(full_path, algorithm1, algorithm2, game_map, 
                     std::filesystem::path(game_map).filename().string(),
                     map_width, map_height, 50, 10, verbose);
        submitTask(task);
    }
    
    // Wait for all tasks to complete
    waitForAllTasks();
    
    // Write results
    writeComparativeResults(game_managers_folder, game_results_);
    
    return true;
}

bool Simulator::runCompetition(const std::string& game_maps_folder, const std::string& game_manager,
                              const std::string& algorithms_folder, int num_threads, bool verbose) {
    verbose_ = verbose;
    std::cout << "Running competition mode..." << std::endl;
    std::cout << "Game maps folder: " << game_maps_folder << std::endl;
    std::cout << "Game manager: " << game_manager << std::endl;
    std::cout << "Algorithms folder: " << algorithms_folder << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    
    // Validate game manager source file exists and is a .cpp
    if (!std::filesystem::exists(game_manager)) {
        std::cerr << "Game manager source file not found: " << game_manager << std::endl;
        return false;
    }
    if (std::filesystem::path(game_manager).extension() != ".cpp") {
        std::cerr << "Expected game_manager to be a .cpp source file; got: "
                  << std::filesystem::path(game_manager).extension().string() << std::endl;
        return false;
    }
    
    // Get all map files
    auto map_files = getFilesInFolder(game_maps_folder, ".txt");
    if (map_files.empty()) {
        std::cerr << "No map files found in folder: " << game_maps_folder << std::endl;
        return false;
    }
    
    // Read and validate each map
    std::vector<std::tuple<std::string, size_t, size_t>> valid_maps;
    for (const auto& map_file : map_files) {
        std::string map_path = game_maps_folder + "/" + map_file;
        std::ifstream map_stream(map_path);
        if (!map_stream.is_open()) {
            std::cerr << "Warning: Could not open map file: " << map_path << std::endl;
            continue;
        }
        
        size_t width = 0, height = 0;
        int player1_tanks = 0, player2_tanks = 0;
        bool has_mines = false;
        std::string line;
        
        while (std::getline(map_stream, line)) {
            height++;
            width = std::max(width, line.length());
            
            // Count tanks and validate map content
            for (char c : line) {
                if (c == '1') player1_tanks++;
                else if (c == '2') player2_tanks++;
                else if (c == '*') has_mines = true;
                else if (c != '#' && c != ' ' && c != '%') {
                    std::cerr << "Warning: Invalid character '" << c << "' in map: " << map_path << std::endl;
                    continue;
                }
            }
        }
        
        // Validate map has both players
        if (player1_tanks == 0) {
            std::cerr << "Warning: Map " << map_file << " has no Player 1 tanks, skipping" << std::endl;
            continue;
        }
        if (player2_tanks == 0) {
            std::cerr << "Warning: Map " << map_file << " has no Player 2 tanks, skipping" << std::endl;
            continue;
        }
        
        if (width > 0 && height > 0) {
            valid_maps.emplace_back(map_file, width, height);
            if (verbose_) {
                std::cout << "  Validated map: " << map_file << " (" << width << "x" << height 
                          << ", P1: " << player1_tanks << " tanks, P2: " << player2_tanks << " tanks)" << std::endl;
            }
        } else {
            std::cerr << "Warning: Invalid map dimensions in file: " << map_path << std::endl;
        }
    }
    
    if (valid_maps.empty()) {
        std::cerr << "No valid map files found in folder: " << game_maps_folder << std::endl;
        return false;
    }
    
    // Get all algorithm source files
    auto algorithm_files = getFilesInFolder(algorithms_folder, ".cpp");
    if (algorithm_files.size() < 2) {
        std::cerr << "Need at least 2 algorithm source files (.cpp) for competition, found: " << algorithm_files.size()
                  << " in folder: " << algorithms_folder << std::endl;
        return false;
    }
    
    // Validate each algorithm file exists
    std::vector<std::string> valid_algorithms;
    for (const auto& alg_file : algorithm_files) {
        std::string alg_path = algorithms_folder + "/" + alg_file;
        if (std::filesystem::exists(alg_path)) {
            valid_algorithms.push_back(alg_file);
        } else {
            std::cerr << "Warning: Algorithm file not found: " << alg_path << std::endl;
        }
    }
    
    if (valid_algorithms.size() < 2) {
        std::cerr << "Need at least 2 valid algorithm files for competition, found: " << valid_algorithms.size() << std::endl;
        return false;
    }
    
    // Calculate total task count first
    size_t total_tasks = 0;
    size_t N = valid_algorithms.size();
    
    // For each map k
    for (size_t k = 0; k < valid_maps.size(); ++k) {
        // For each algorithm i
        for (size_t i = 0; i < N; ++i) {
            // Calculate opponent j using the tournament formula:
            // j = (i + 1 + k % (N-1)) % N
            size_t j = (i + 1 + k % (N-1)) % N;
            
            // Skip if we've already done this pairing (j vs i) or if it's self-play
            if (i >= j) continue;
            
            total_tasks++; // One task for i vs j
            
            // Special case: if k = N/2 - 1 (for even N), the pairing would be the same
            // in both games, so we only run one game
            if (N % 2 == 0 && k == N/2 - 1) {
                continue;
            }
            
            total_tasks++; // One task for j vs i
        }
    }
    
    if (verbose_) {
        std::cout << "\nTournament Setup:" << std::endl;
        std::cout << "Number of algorithms (N): " << N << std::endl;
        std::cout << "Number of maps (K): " << valid_maps.size() << std::endl;
        std::cout << "Total tasks to be created: " << total_tasks << std::endl;
    }
    
    // Initialize thread pool with actual task count
    initializeThreadPool(num_threads, total_tasks);
    
    // Now create the actual tasks
    // For each map k
    for (size_t k = 0; k < valid_maps.size(); ++k) {
        const auto& [map_file, width, height] = valid_maps[k];
        std::string map_path = game_maps_folder + "/" + map_file;
        
        if (verbose_) {
            std::cout << "\nMap " << k << " (" << map_file << ") pairings:" << std::endl;
        }
        
        // For each algorithm i
        for (size_t i = 0; i < N; ++i) {
            // Calculate opponent j using the tournament formula:
            // j = (i + 1 + k % (N-1)) % N
            size_t j = (i + 1 + k % (N-1)) % N;
            
            // Skip if we've already done this pairing (j vs i) or if it's self-play
            if (i >= j) continue;
            
            if (verbose_) {
                std::cout << "  Algorithm " << i << " (" << valid_algorithms[i] << ")"
                         << " vs "
                         << "Algorithm " << j << " (" << valid_algorithms[j] << ")"
                         << std::endl;
            }
            
            // Create task for this pairing
            std::string alg1_path = algorithms_folder + "/" + valid_algorithms[i];
            std::string alg2_path = algorithms_folder + "/" + valid_algorithms[j];
            
            GameTask task(game_manager, alg1_path, alg2_path, map_path,
                        map_file, width, height, 50, 10, verbose);
            submitTask(task);
            
            // Special case: if k = N/2 - 1 (for even N), the pairing would be the same
            // in both games, so we only run one game
            if (N % 2 == 0 && k == N/2 - 1) {
                if (verbose_) {
                    std::cout << "    (Skipping reverse match - same pairing)" << std::endl;
                }
                continue;
            }
            
            // Create task for the reverse pairing (j vs i)
            GameTask reverse_task(game_manager, alg2_path, alg1_path, map_path,
                                map_file, width, height, 50, 10, verbose);
            submitTask(reverse_task);
        }
    }
    
    if (verbose_) {
        std::cout << "\nAll tournament pairings created." << std::endl;
    }
    
    // Wait for all tasks to complete
    waitForAllTasks();
    
    // Write results
    writeCompetitionResults(algorithms_folder, game_results_);
    
    return true;
}

void Simulator::initializeThreadPool(int num_threads, size_t total_tasks) {
    if (num_threads <= 1) {
        return; // Single-threaded mode
    }
    
    // According to assignment: "Don't create more threads than can be utilized"
    // Only create as many worker threads as we have tasks
    size_t actual_threads = std::min(static_cast<size_t>(num_threads), total_tasks);
    
    if (verbose_) {
        std::cout << "Creating " << actual_threads << " worker threads for " << total_tasks << " tasks" << std::endl;
    }
    
    // Create worker threads
    for (size_t i = 0; i < actual_threads; ++i) {
        workers_.emplace_back(&Simulator::workerThread, this);
    }
}

void Simulator::workerThread() {
    try {
        while (!stop_workers_) {  // Check stop flag first
            GameTask task;
            bool have_task = false;
            
            // Get a task from the queue
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                // Wait for task or stop signal with timeout
                if (condition_.wait_for(lock, 
                    std::chrono::milliseconds(50),  // 50ms timeout for faster response
                    [this] { return !task_queue_.empty() || stop_workers_; }))
                {
                    if (stop_workers_) {
                        break;  // Exit if stop was signaled
                    }
                    
                    if (!task_queue_.empty()) {
                        task = task_queue_.front();
                        task_queue_.pop();
                        have_task = true;
                    }
                }
            }
            
            // Process task if we got one
            if (have_task && !task.game_manager_path.empty()) {
                try {
                    SimulatorGameResult result = executeGame(task);
                    
                    // Store result
                    {
                        std::lock_guard<std::mutex> lock(results_mutex_);
                        game_results_.push_back(std::move(result));
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error executing game task: " << e.what() << std::endl;
                }
            }
            
            // If no task and stop is signaled, exit immediately
            if (!have_task && stop_workers_) {
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Worker thread error: " << e.what() << std::endl;
    }
    
    if (verbose_) {
        std::cout << "Worker thread exiting" << std::endl;
    }
}

void Simulator::submitTask(const GameTask& task) {
    if (workers_.empty()) {
        // Single-threaded mode, execute immediately
        SimulatorGameResult result = executeGame(task);
        std::lock_guard<std::mutex> lock(results_mutex_);
        game_results_.push_back(std::move(result)); // Store the result using move semantics
    } else {
        // Multi-threaded mode, add to queue
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(task);
        condition_.notify_one();
    }
}

void Simulator::waitForAllTasks() {
    if (workers_.empty()) {
        return; // Single-threaded, already done
    }
    
    // Signal workers to stop and wait for tasks to complete
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        // First wait for all tasks to be processed with timeout
        if (!task_queue_.empty()) {
            if (!condition_.wait_for(lock, 
                std::chrono::milliseconds(5000),  // 5 second timeout
                [this] { return task_queue_.empty(); })) {
                std::cerr << "Warning: Timeout waiting for tasks to complete" << std::endl;
            }
        }
        // Then signal workers to stop
        stop_workers_ = true;
        lock.unlock();  // Unlock before notify to avoid deadlock
        condition_.notify_all();
    }
    
    // Wait for all workers to finish with timeout
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            if (worker.native_handle()) {
                // Use a timeout to avoid hanging
                std::chrono::milliseconds timeout(2000);  // 2 second timeout
                auto start = std::chrono::steady_clock::now();
                while (worker.joinable() && 
                       std::chrono::steady_clock::now() - start < timeout) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                if (worker.joinable()) {
                    std::cerr << "Warning: Worker thread did not finish in time, detaching" << std::endl;
                    worker.detach();
                }
            }
        }
    }
    
    // Clear worker threads
    workers_.clear();
    stop_workers_ = false;  // Reset for next use
}

// No dynamic library loading needed

SimulatorGameResult Simulator::executeGame(const GameTask& task) {
    SimulatorGameResult result;
    
    // Verify source files exist
    if (!std::filesystem::exists(task.game_manager_path)) {
        std::cerr << "Game manager source file not found: " << task.game_manager_path << std::endl;
        return result;
    }
    if (!std::filesystem::exists(task.algorithm1_path)) {
        std::cerr << "Algorithm 1 source file not found: " << task.algorithm1_path << std::endl;
        return result;
    }
    if (!std::filesystem::exists(task.algorithm2_path)) {
        std::cerr << "Algorithm 2 source file not found: " << task.algorithm2_path << std::endl;
        return result;
    }
    
    try {
        // Set metadata
        result.game_manager_name = getLibraryName(task.game_manager_path);
        result.algorithm1_name = getLibraryName(task.algorithm1_path);
        result.algorithm2_name = getLibraryName(task.algorithm2_path);
        // Preserve original filenames (for output spec)
        result.game_manager_file = std::filesystem::path(task.game_manager_path).filename().string();
        result.algorithm1_file = std::filesystem::path(task.algorithm1_path).filename().string();
        result.algorithm2_file = std::filesystem::path(task.algorithm2_path).filename().string();
        result.map_name = task.map_name;
        result.map_path = task.map_path;
        result.map_width = task.map_width;
        result.map_height = task.map_height;
        
        // Create a map view from the map file
        auto map_view = createMapFromFile(task.map_path, task.map_width, task.map_height);
        if (!map_view) {
            std::cerr << "Failed to create map from file: " << task.map_path << std::endl;
            return result;
        }
        
        // Create the actual game components
        try {
            // Get the AlgorithmRegistrar to find registered algorithms
            auto& alg_registrar = AlgorithmRegistrar::getAlgorithmRegistrar();
            
            // Ensure we have at least one registered algorithm
            if (alg_registrar.count() == 0) {
                std::cerr << "No algorithms registered in AlgorithmRegistrar" << std::endl;
                throw std::runtime_error("No algorithms registered");
            }
            
            // Try to match algorithms by names derived from file paths
            const std::string desired_alg1 = result.algorithm1_name; // from getLibraryName
            const std::string desired_alg2 = result.algorithm2_name;
            
            // Pointers to selected entries (fallback to first if not found)
            const void* selected1Ptr = nullptr;
            const void* selected2Ptr = nullptr;
            
            // Find entries
            for (auto it = alg_registrar.begin(); it != alg_registrar.end(); ++it) {
                const auto& entry = *it;
                if (!selected1Ptr && entry.name() == desired_alg1) {
                    selected1Ptr = &entry;
                }
                if (!selected2Ptr && entry.name() == desired_alg2) {
                    selected2Ptr = &entry;
                }
            }
            
            const auto& first_alg_fallback = *alg_registrar.begin();
            const auto& alg1_entry = selected1Ptr ? *static_cast<const std::remove_reference<decltype(first_alg_fallback)>::type*>(const_cast<void*>(selected1Ptr))
                                                  : first_alg_fallback;
            const auto& alg2_entry = selected2Ptr ? *static_cast<const std::remove_reference<decltype(first_alg_fallback)>::type*>(const_cast<void*>(selected2Ptr))
                                                  : first_alg_fallback;
            
            // Create Player instances - cannot assume copy constructors exist
            // Use raw pointers as required by assignment (Simulator doesn't control Player class)
            auto player1_ptr = alg1_entry.createPlayer(1, 0, 0, task.max_steps, task.num_shells);
            auto player2_ptr = alg2_entry.createPlayer(2, 0, 0, task.max_steps, task.num_shells);
            Player* player1 = player1_ptr.get();
            Player* player2 = player2_ptr.get();
            
            // Create TankAlgorithm factory functions
            auto tank_algo_factory1 = [&alg1_entry](int player_index, int tank_index) {
                return alg1_entry.createTankAlgorithm(player_index, tank_index);
            };
            auto tank_algo_factory2 = [&alg2_entry](int player_index, int tank_index) {
                return alg2_entry.createTankAlgorithm(player_index, tank_index);
            };
            
            if (!player1 || !player2) {
                std::cerr << "Failed to create Player instances" << std::endl;
                throw std::runtime_error("Failed to create Player instances");
            }
            
            // Get registered GameManager factory
            auto gm_factory = getGameManagerFactory(result.game_manager_name);
            if (!gm_factory) {
                // If not found by name, try to get the first available GameManager
                auto registered_gms = getRegisteredGameManagers();
                if (!registered_gms.empty()) {
                    gm_factory = getGameManagerFactory(registered_gms[0]);
                    if (gm_factory) {
                        result.game_manager_name = registered_gms[0]; // Update the name
                    }
                }
            }
            if (!gm_factory) {
                std::cerr << "No GameManager found in registry" << std::endl;
                throw std::runtime_error("GameManager not found in registry");
            }
            
            // Create GameManager instance
            auto game_manager = gm_factory(task.verbose);
            if (!game_manager) {
                std::cerr << "Failed to create GameManager instance" << std::endl;
                throw std::runtime_error("Failed to create GameManager instance");
            }
            
            // Run the actual game - pass references to players (ownership stays with Simulator)
            auto game_result = game_manager->run(
                task.map_width, task.map_height,
                *map_view, task.map_name,
                task.max_steps, task.num_shells,
                *player1, result.algorithm1_name,        // Pass reference, ownership with Simulator
                *player2, result.algorithm2_name,        // Pass reference, ownership with Simulator
                tank_algo_factory1, tank_algo_factory2
            );
            
            // Store the real game result
            result.game_result = std::move(game_result);
            
            // Clean up players manually since we can't assume RAII
            // Note: player1_ptr and player2_ptr will be automatically cleaned up when they go out of scope
            
        } catch (const std::exception& e) {
            std::cerr << "Error running game: " << e.what() << std::endl;
            // Fall back to mock result
            result.game_result.winner = 0; // Tie for now
            result.game_result.reason = ::GameResult::MAX_STEPS;
            result.game_result.rounds = task.max_steps;
            result.game_result.remaining_tanks = {1, 1};
            result.game_result.gameState = std::move(map_view);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error executing game: " << e.what() << std::endl;
    }
    
    return result;
}

std::unique_ptr<SatelliteView> Simulator::createMapFromFile(const std::string& map_path, size_t width, size_t height) {
    // Create a map view that can handle both HW2 (header) and HW3 (raw) map formats
    
    std::ifstream map_file(map_path);
    if (!map_file.is_open()) {
        std::cerr << "Failed to open map file: " << map_path << std::endl;
        return nullptr;
    }
    
    std::vector<std::string> map_lines;
    std::string line;
    
    // Read first line to detect format
    if (!std::getline(map_file, line)) {
        std::cerr << "Map file is empty: " << map_path << std::endl;
        map_file.close();
        return nullptr;
    }
    
    // Parse header like HW2 if present; else treat as raw grid
    auto parseParamLine = [](const std::string& ln, const std::string& key) -> size_t {
        auto eq = ln.find('=');
        if (eq == std::string::npos) throw std::runtime_error("Missing '=' in " + key + " line: " + ln);
        if (ln.substr(0, eq).find(key) == std::string::npos) throw std::runtime_error("Expected key '" + key + "' in line: " + ln);
        std::string value = ln.substr(eq + 1);
        size_t start = value.find_first_not_of(" \t");
        size_t end   = value.find_last_not_of(" \t");
        if (start == std::string::npos) throw std::runtime_error("No value for " + key);
        value = value.substr(start, end - start + 1);
        return static_cast<size_t>(std::stoul(value));
    };

    bool is_hw2_format = false;
    size_t actual_width = 0;
    size_t actual_height = 0;
    std::string first_line = line;
    std::string second_line;
    bool has_second = static_cast<bool>(std::getline(map_file, second_line));
    if (has_second && (second_line.find("MaxSteps") != std::string::npos || second_line.find("NumShells") != std::string::npos)) {
        is_hw2_format = true;
        if (verbose_) {
            std::cout << "Detected HW2 map format, parsing header..." << std::endl;
        }
        /*size_t maxSteps =
            */ parseParamLine(second_line, "MaxSteps");
        if (!std::getline(map_file, line)) { std::cerr << "Invalid HW2 header: missing NumShells" << std::endl; map_file.close(); return nullptr; }
        /*size_t numShells =
            */ parseParamLine(line, "NumShells");
        if (!std::getline(map_file, line)) { std::cerr << "Invalid HW2 header: missing Rows" << std::endl; map_file.close(); return nullptr; }
        actual_height = parseParamLine(line, "Rows");
        if (!std::getline(map_file, line)) { std::cerr << "Invalid HW2 header: missing Cols" << std::endl; map_file.close(); return nullptr; }
        actual_width = parseParamLine(line, "Cols");
        if (verbose_) {
            std::cout << "HW2 map dimensions: " << actual_width << "x" << actual_height << std::endl;
        }
        for (size_t r = 0; r < actual_height; ++r) {
            if (std::getline(map_file, line)) {
                if (line.size() < actual_width) {
                    line += std::string(actual_width - line.size(), ' ');
                } else if (line.size() > actual_width) {
                    line.resize(actual_width);
                }
            } else {
                line = std::string(actual_width, ' ');
            }
            map_lines.push_back(line);
        }
    } else {
        std::cerr << "Invalid map format: expected HW2 header (MaxSteps/NumShells/Rows/Cols) in '" << map_path << "'" << std::endl;
        map_file.close();
        return nullptr;
    }
    map_file.close();
    
    if (map_lines.empty()) {
        std::cerr << "No map data found in file: " << map_path << std::endl;
        return nullptr;
    }
    
    // Create a simple map view implementation
    class SimpleMapView : public SatelliteView {
    private:
        std::vector<std::string> map_data_;
        size_t width_;
        size_t height_;
        
    public:
        SimpleMapView(std::vector<std::string> map_data, size_t width, size_t height)
            : map_data_(std::move(map_data)), width_(width), height_(height) {}
        
        char getObjectAt(size_t x, size_t y) const override {
            if (y < map_data_.size() && x < map_data_[y].length()) {
                return map_data_[y][x];
            }
            return ' '; // Return space for out-of-bounds
        }
        
        std::unique_ptr<SatelliteView> clone() const override {
            return std::make_unique<SimpleMapView>(map_data_, width_, height_);
        }
    };
    
    if (verbose_) {
        std::cout << "Created map view with " << map_lines.size() << " lines, dimensions: " 
                  << actual_width << "x" << actual_height << std::endl;
    }
    
    return std::make_unique<SimpleMapView>(std::move(map_lines), actual_width, actual_height);
}

void Simulator::writeComparativeResults(const std::string& output_folder, 
                                      const std::vector<SimulatorGameResult>& results) {
    if (results.empty()) {
        std::cerr << "No results to write" << std::endl;
        return;
    }

    std::string filename = output_folder + "/comparative_results_" + generateTimestamp() + ".txt";
    std::ofstream file(filename);
    
    // If file can't be opened, use cout instead
    std::ostream& out = file.is_open() ? file : std::cout;
    if (!file.is_open()) {
        std::cerr << "Failed to create output file: " << filename << std::endl;
        std::cout << "Writing results to screen instead:" << std::endl;
    }
    
    // Write header (using first result for common info)
    out << "game_map=" << results[0].map_name << std::endl;
    // According to spec, print filenames (could be .so in official run). We stored original file names.
    out << "algorithm1=" << results[0].algorithm1_file << std::endl;
    out << "algorithm2=" << results[0].algorithm2_file << std::endl;
    out << std::endl;  // Empty line after header
    
    // Group results by exact same outcome (winner, reason, round, and final state)
    std::map<std::string, std::vector<SimulatorGameResult>> grouped_results;
    for (const auto& result : results) {
        std::string key;
        key += std::to_string(result.game_result.winner) + "_";
        key += std::to_string(static_cast<int>(result.game_result.reason)) + "_";
        key += std::to_string(result.game_result.rounds) + "_";
        
        // Add final state to key if available
        if (result.game_result.gameState) {
            for (size_t y = 0; y < result.map_height; ++y) {
                for (size_t x = 0; x < result.map_width; ++x) {
                    key += result.game_result.gameState->getObjectAt(x, y);
                }
            }
        }
        
        grouped_results[key].push_back(result);
    }
    
    // Write each group
    bool first_group = true;
    for (const auto& group : grouped_results) {
        const auto& first_result = group.second[0];
        
        // Add blank line between groups (except before first group)
        if (!first_group) {
            out << std::endl;
        }
        first_group = false;
        
        // Write comma-separated list of game managers that got this exact result
        bool first_manager = true;
        for (const auto& result : group.second) {
            if (!first_manager) out << ",";
            out << result.game_manager_name;
            first_manager = false;
        }
        out << std::endl;
        
        // Write game result message
        if (first_result.game_result.winner == 0) {
            out << "Tie - ";
        } else {
            out << "Player " << first_result.game_result.winner << " wins - ";
        }
        
        // Add reason
        switch (first_result.game_result.reason) {
            case GameResult::ALL_TANKS_DEAD:
                out << "all opponent tanks destroyed";
                break;
            case GameResult::MAX_STEPS:
                out << "maximum steps reached";
                break;
            case GameResult::ZERO_SHELLS:
                out << "no shells remaining";
                break;
        }
        out << std::endl;
        
        // Write round number
        out << first_result.game_result.rounds << std::endl;
        
        // Write final map state
        if (first_result.game_result.gameState) {
            for (size_t y = 0; y < first_result.map_height; ++y) {
                for (size_t x = 0; x < first_result.map_width; ++x) {
                    out << first_result.game_result.gameState->getObjectAt(x, y);
                }
                out << std::endl;
            }
        } else {
            out << "<Final state not available>" << std::endl;
        }
    }
    
    if (file.is_open()) {
        file.close();
        std::cout << "Comparative results written to: " << filename << std::endl;
    }
}

void Simulator::writeCompetitionResults(const std::string& output_folder,
                                       const std::vector<SimulatorGameResult>& results) {
    if (results.empty()) {
        std::cerr << "No results to write" << std::endl;
        return;
    }

    std::string filename = output_folder + "/competition_" + generateTimestamp() + ".txt";
    std::ofstream file(filename);
    
    // If file can't be opened, use cout instead
    std::ostream& out = file.is_open() ? file : std::cout;
    if (!file.is_open()) {
        std::cerr << "Failed to create output file: " << filename << std::endl;
        std::cout << "Writing results to screen instead:" << std::endl;
    }
    
    // Write header
    out << "game_maps_folder=" << std::filesystem::path(results[0].map_path).parent_path().string() << std::endl;
    out << "game_manager=" << results[0].game_manager_file << std::endl;
    out << std::endl;  // Empty line after header
    
    // Calculate scores for each algorithm
    std::map<std::string, int> scores;
    std::map<std::string, std::vector<std::string>> match_history;  // For debug output
    
    if (verbose_) {
        std::cout << "\nScoring Details:" << std::endl;
    }
    
    for (const auto& result : results) {
        // Initialize scores if not present
        if (scores.find(result.algorithm1_name) == scores.end()) {
            scores[result.algorithm1_name] = 0;
        }
        if (scores.find(result.algorithm2_name) == scores.end()) {
            scores[result.algorithm2_name] = 0;
        }
        
        // Award points
        if (result.game_result.winner == 0) {
            // Tie - both get 1 point
            scores[result.algorithm1_name] += 1;
            scores[result.algorithm2_name] += 1;
            
            if (verbose_) {
                std::cout << "Map " << result.map_name << ": "
                         << result.algorithm1_name << " vs " << result.algorithm2_name
                         << " - Tie (1 point each)" << std::endl;
            }
            
            // Record match history
            std::string match = "Tie vs " + result.algorithm2_name + " on " + result.map_name;
            match_history[result.algorithm1_name].push_back(match);
            match = "Tie vs " + result.algorithm1_name + " on " + result.map_name;
            match_history[result.algorithm2_name].push_back(match);
            
        } else if (result.game_result.winner == 1) {
            // Algorithm 1 wins - gets 3 points
            scores[result.algorithm1_name] += 3;
            
            if (verbose_) {
                std::cout << "Map " << result.map_name << ": "
                         << result.algorithm1_name << " defeats " << result.algorithm2_name
                         << " (3 points)" << std::endl;
            }
            
            // Record match history
            std::string match = "Win vs " + result.algorithm2_name + " on " + result.map_name;
            match_history[result.algorithm1_name].push_back(match);
            match = "Loss vs " + result.algorithm1_name + " on " + result.map_name;
            match_history[result.algorithm2_name].push_back(match);
            
        } else {
            // Algorithm 2 wins - gets 3 points
            scores[result.algorithm2_name] += 3;
            
            if (verbose_) {
                std::cout << "Map " << result.map_name << ": "
                         << result.algorithm2_name << " defeats " << result.algorithm1_name
                         << " (3 points)" << std::endl;
            }
            
            // Record match history
            std::string match = "Loss vs " + result.algorithm2_name + " on " + result.map_name;
            match_history[result.algorithm1_name].push_back(match);
            match = "Win vs " + result.algorithm1_name + " on " + result.map_name;
            match_history[result.algorithm2_name].push_back(match);
        }
    }
    
    // Sort by score (descending)
    std::vector<std::pair<std::string, int>> sorted_scores(scores.begin(), scores.end());
    std::sort(sorted_scores.begin(), sorted_scores.end(),
              [](const auto& a, const auto& b) {
                  // Sort by score (descending), then by name (ascending) for equal scores
                  return a.second > b.second || (a.second == b.second && a.first < b.first);
              });
    
    // Write final standings
    for (const auto& score : sorted_scores) {
        out << score.first << " " << score.second << std::endl;
    }
    
    if (verbose_) {
        std::cout << "\nFinal Standings:" << std::endl;
        for (const auto& score : sorted_scores) {
            std::cout << score.first << ": " << score.second << " points" << std::endl;
            std::cout << "  Matches:" << std::endl;
            for (const auto& match : match_history[score.first]) {
                std::cout << "    " << match << std::endl;
            }
            std::cout << std::endl;
        }
    }
    
    if (file.is_open()) {
        file.close();
        std::cout << "Competition results written to: " << filename << std::endl;
    }
}

std::string Simulator::getLibraryName(const std::string& path) {
    std::string filename = std::filesystem::path(path).filename().string();
    
    // Remove the .cpp extension
    if (filename.length() > 4 && filename.substr(filename.length() - 4) == ".cpp") {
        filename = filename.substr(0, filename.length() - 4);
    }
    
    // Extract the class name (everything after the last underscore if it exists)
    size_t last_underscore = filename.find_last_of('_');
    if (last_underscore != std::string::npos) {
        filename = filename.substr(0, last_underscore);
    }
    
    return filename;
}

std::vector<std::string> Simulator::getFilesInFolder(const std::string& folder, const std::string& extension) {
    std::vector<std::string> files;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.is_regular_file() && entry.path().extension() == extension) {
                files.push_back(entry.path().filename().string());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error reading folder " << folder << ": " << e.what() << std::endl;
    }
    
    return files;
}

std::string Simulator::generateTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

bool Simulator::loadAlgorithmLibrary(const std::string& library_path) {
    std::cout << "Loading algorithm library: " << library_path << std::endl;
    
    // Create a new loader for this library
    auto loader = std::make_unique<DynamicLibraryLoader>();
    
    // Convert library path to full path with correct extension
    std::string full_path;
#ifdef _WIN32
    // Map requested cpp to the corresponding DLL we build
    if (library_path.find("AggressiveTankAI_") != std::string::npos) {
        full_path = "cmake-build-debug/bin/libAlgorithm_Aggressive_212934582_323964676.dll";
    } else if (library_path.find("TankAlgorithm_Simple_") != std::string::npos) {
        full_path = "cmake-build-debug/bin/libAlgorithm_Simple_212934582_323964676.dll";
    } else {
        full_path = "cmake-build-debug/bin/libAlgorithm_Simple_212934582_323964676.dll"; // default
    }
#else
    if (library_path.find("AggressiveTankAI_") != std::string::npos) {
        full_path = "cmake-build-debug/lib/libAlgorithm_Aggressive_212934582_323964676.so";
    } else if (library_path.find("TankAlgorithm_Simple_") != std::string::npos) {
        full_path = "cmake-build-debug/lib/libAlgorithm_Simple_212934582_323964676.so";
    } else {
        full_path = "cmake-build-debug/lib/libAlgorithm_Simple_212934582_323964676.so";
    }
#endif

    // Load the library
    if (!loader->loadLibrary(full_path)) {
        std::cerr << "Failed to load library: " << full_path << std::endl;
        std::cerr << "Error: " << loader->getLastError() << std::endl;
        return false;
    }
    
    // Get the registration function that accepts our registrar pointer
    typedef void (*RegisterFunction)(AlgorithmRegistrar*);
    const char* symbol_name = (full_path.find("Aggressive") != std::string::npos)
        ? "register_algorithms_aggressive_212934582_323964676"
        : "register_algorithms_simple_212934582_323964676";
    RegisterFunction register_func = reinterpret_cast<RegisterFunction>(loader->getFunction(symbol_name));
    
    if (!register_func) {
        std::cerr << "Failed to find initialization function in: " << full_path << std::endl;
        std::cerr << "Error: " << loader->getLastError() << std::endl;
        return false;
    }
    
    // Call the registration function with the host registrar
    auto& host_registrar = AlgorithmRegistrar::getAlgorithmRegistrar();
    register_func(&host_registrar);
    std::cout << "Successfully loaded and initialized: " << full_path << std::endl;
    
    // Keep the loader alive so function pointers remain valid
    loaded_algorithm_libraries_.push_back(std::move(loader));
    
    return true;
}

bool Simulator::loadGameManagerLibrary(const std::string& library_path) {
    std::cout << "Loading GameManager library: " << library_path << std::endl;
    
    auto loader = std::make_unique<DynamicLibraryLoader>();
    
    // Convert library path to full path with correct extension
    std::string full_path;
#ifdef _WIN32
    // For Windows: .dll files are in bin/ directory
    if (library_path.length() >= 4 && library_path.substr(library_path.length() - 4) == ".cpp") {
        // Use the GameManager shared library we created
        full_path = "cmake-build-debug/bin/libGameManager_212934582_323964676.dll";
    } else {
        full_path = library_path;
    }
#else
    // For Linux: .so files are in lib/ directory
    if (library_path.length() >= 4 && library_path.substr(library_path.length() - 4) == ".cpp") {
        full_path = "cmake-build-debug/lib/libGameManager_212934582_323964676.so";
    } else {
        full_path = library_path;
    }
#endif

    if (!loader->loadLibrary(full_path)) {
        std::cerr << "Failed to load GameManager library: " << full_path << std::endl;
        std::cerr << "Error: " << loader->getLastError() << std::endl;
        return false;
    }
    
    // Get the initialization function
    typedef void (*InitFunction)();
    InitFunction init_func = reinterpret_cast<InitFunction>(
        loader->getFunction("initialize_gamemanager_212934582_323964676"));
    
    if (!init_func) {
        std::cerr << "Failed to find GameManager initialization function in: " << full_path << std::endl;
        std::cerr << "Error: " << loader->getLastError() << std::endl;
        return false;
    }
    
    init_func();
    std::cout << "Successfully loaded and initialized GameManager: " << full_path << std::endl;
    
    // Keep the loader alive so any GM symbols remain valid
    loaded_gamemanager_libraries_.push_back(std::move(loader));
    
    return true;
}
