
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
+#include <cstdlib>

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
diff --git a/Simulator/Simulator.cpp b/Simulator/Simulator.cpp
index 8aa2c091c499227ab46477b4ff5ec70cb92a740d..1c5d41d13fc36c3a5e5f24e7606b3e211c1cab76 100644
--- a/Simulator/Simulator.cpp
+++ b/Simulator/Simulator.cpp
@@ -115,96 +116,119 @@ bool Simulator::runComparative(const std::string& game_map, const std::string& g

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

-    // Get all game manager source files in the folder (.cpp)
-    auto game_manager_files = getFilesInFolder(game_managers_folder, ".cpp");
+    // Determine dynamic library extension
+#ifdef _WIN32
+    const std::string gm_ext = ".dll";
+#else
+    const std::string gm_ext = ".so";
+#endif
+
+    // Get all game manager libraries in the folder
+    auto game_manager_files = getFilesInFolder(game_managers_folder, gm_ext);
     if (game_manager_files.empty()) {
-        std::cerr << "No source files (.cpp) found in game managers folder: " << game_managers_folder << "\n"
-                  << "If you intended to pass a static library, note this simulator expects .cpp sources and links them directly." << std::endl;
+        std::cerr << "No game manager libraries (*" << gm_ext << ") found in folder: "
+                  << game_managers_folder << std::endl;
         return false;
     }
-
+
+    // Load all game manager libraries
+    for (const auto& gm_file : game_manager_files) {
+        std::string full_path = game_managers_folder + "/" + gm_file;
+        if (!loadGameManagerLibrary(full_path)) {
+            std::cerr << "Failed to load game manager library: " << full_path << std::endl;
+            return false;
+        }
+    }
+
     // Initialize thread pool
     initializeThreadPool(num_threads, game_manager_files.size());
-
+
     // Create tasks for each game manager
     for (const auto& gm_file : game_manager_files) {
         std::string full_path = game_managers_folder + "/" + gm_file;
-        GameTask task(full_path, algorithm1, algorithm2, game_map,
+        GameTask task(full_path, algorithm1, algorithm2, game_map,
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
-
-    // Validate game manager source file exists and is a .cpp
-    if (!std::filesystem::exists(game_manager)) {
-        std::cerr << "Game manager source file not found: " << game_manager << std::endl;
+
+    // Determine dynamic library extension
+#ifdef _WIN32
+    const std::string lib_ext = ".dll";
+#else
+    const std::string lib_ext = ".so";
+#endif
+
+    // Validate and load game manager library
+    if (!std::filesystem::exists(game_manager) ||
+        std::filesystem::path(game_manager).extension() != lib_ext) {
+        std::cerr << "Game manager library not found or wrong extension: " << game_manager << std::endl;
         return false;
     }
-    if (std::filesystem::path(game_manager).extension() != ".cpp") {
-        std::cerr << "Expected game_manager to be a .cpp source file; got: "
-                  << std::filesystem::path(game_manager).extension().string() << std::endl;
+    if (!loadGameManagerLibrary(game_manager)) {
+        std::cerr << "Failed to load GameManager library: " << game_manager << std::endl;
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

diff --git a/Simulator/Simulator.cpp b/Simulator/Simulator.cpp
index 8aa2c091c499227ab46477b4ff5ec70cb92a740d..1c5d41d13fc36c3a5e5f24e7606b3e211c1cab76 100644
--- a/Simulator/Simulator.cpp
+++ b/Simulator/Simulator.cpp
@@ -228,71 +252,72 @@ bool Simulator::runCompetition(const std::string& game_maps_folder, const std::s
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

-    // Get all algorithm source files
-    auto algorithm_files = getFilesInFolder(algorithms_folder, ".cpp");
+    // Get all algorithm libraries
+    auto algorithm_files = getFilesInFolder(algorithms_folder, lib_ext);
     if (algorithm_files.size() < 2) {
-        std::cerr << "Need at least 2 algorithm source files (.cpp) for competition, found: " << algorithm_files.size()
-                  << " in folder: " << algorithms_folder << std::endl;
+        std::cerr << "Need at least 2 algorithm libraries (*" << lib_ext << ") for competition, found: "
+                  << algorithm_files.size() << " in folder: " << algorithms_folder << std::endl;
         return false;
     }
-
-    // Validate each algorithm file exists
+
+    // Load each algorithm library
     std::vector<std::string> valid_algorithms;
     for (const auto& alg_file : algorithm_files) {
         std::string alg_path = algorithms_folder + "/" + alg_file;
-        if (std::filesystem::exists(alg_path)) {
-            valid_algorithms.push_back(alg_file);
-        } else {
-            std::cerr << "Warning: Algorithm file not found: " << alg_path << std::endl;
+        if (!loadAlgorithmLibrary(alg_path)) {
+            std::cerr << "Warning: Failed to load algorithm library: " << alg_path << std::endl;
+            continue;
         }
+        valid_algorithms.push_back(alg_file);
     }
-
+
     if (valid_algorithms.size() < 2) {
-        std::cerr << "Need at least 2 valid algorithm files for competition, found: " << valid_algorithms.size() << std::endl;
+        std::cerr << "Need at least 2 valid algorithm libraries for competition, found: "
+                  << valid_algorithms.size() << std::endl;
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
diff --git a/Simulator/Simulator.cpp b/Simulator/Simulator.cpp
index 8aa2c091c499227ab46477b4ff5ec70cb92a740d..1c5d41d13fc36c3a5e5f24e7606b3e211c1cab76 100644
--- a/Simulator/Simulator.cpp
+++ b/Simulator/Simulator.cpp
@@ -444,108 +469,96 @@ void Simulator::workerThread() {
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
-
-    // Signal workers to stop and wait for tasks to complete
-    {
+
+    // Wait for all queued tasks to be processed
+    while (true) {
         std::unique_lock<std::mutex> lock(queue_mutex_);
-        // First wait for all tasks to be processed with timeout
-        if (!task_queue_.empty()) {
-            if (!condition_.wait_for(lock,
-                std::chrono::milliseconds(5000),  // 5 second timeout
-                [this] { return task_queue_.empty(); })) {
-                std::cerr << "Warning: Timeout waiting for tasks to complete" << std::endl;
-            }
+        if (task_queue_.empty()) {
+            break;
         }
-        // Then signal workers to stop
+        lock.unlock();
+        std::this_thread::sleep_for(std::chrono::milliseconds(10));
+    }
+
+    // Signal workers to stop
+    {
+        std::lock_guard<std::mutex> lock(queue_mutex_);
         stop_workers_ = true;
-        lock.unlock();  // Unlock before notify to avoid deadlock
-        condition_.notify_all();
     }
-
-    // Wait for all workers to finish with timeout
+    condition_.notify_all();
+
+    // Join all workers (blocking until completion)
     for (auto& worker : workers_) {
         if (worker.joinable()) {
-            if (worker.native_handle()) {
-                // Use a timeout to avoid hanging
-                std::chrono::milliseconds timeout(2000);  // 2 second timeout
-                auto start = std::chrono::steady_clock::now();
-                while (worker.joinable() &&
-                       std::chrono::steady_clock::now() - start < timeout) {
-                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
-                }
-                if (worker.joinable()) {
-                    std::cerr << "Warning: Worker thread did not finish in time, detaching" << std::endl;
-                    worker.detach();
-                }
-            }
+            worker.join();
         }
     }
-
-    // Clear worker threads
+
+    // Clear worker threads and reset flag for next run
     workers_.clear();
-    stop_workers_ = false;  // Reset for next use
+    stop_workers_ = false;
 }

 // No dynamic library loading needed

 SimulatorGameResult Simulator::executeGame(const GameTask& task) {
     SimulatorGameResult result;

-    // Verify source files exist
+    // Verify library files exist
     if (!std::filesystem::exists(task.game_manager_path)) {
-        std::cerr << "Game manager source file not found: " << task.game_manager_path << std::endl;
+        std::cerr << "Game manager library not found: " << task.game_manager_path << std::endl;
         return result;
     }
     if (!std::filesystem::exists(task.algorithm1_path)) {
-        std::cerr << "Algorithm 1 source file not found: " << task.algorithm1_path << std::endl;
+        std::cerr << "Algorithm 1 library not found: " << task.algorithm1_path << std::endl;
         return result;
     }
     if (!std::filesystem::exists(task.algorithm2_path)) {
-        std::cerr << "Algorithm 2 source file not found: " << task.algorithm2_path << std::endl;
+        std::cerr << "Algorithm 2 library not found: " << task.algorithm2_path << std::endl;
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
diff --git a/Simulator/Simulator.cpp b/Simulator/Simulator.cpp
index 8aa2c091c499227ab46477b4ff5ec70cb92a740d..1c5d41d13fc36c3a5e5f24e7606b3e211c1cab76 100644
--- a/Simulator/Simulator.cpp
+++ b/Simulator/Simulator.cpp
@@ -980,176 +993,135 @@ void Simulator::writeCompetitionResults(const std::string& output_folder,
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
-
-    // Remove the .cpp extension
-    if (filename.length() > 4 && filename.substr(filename.length() - 4) == ".cpp") {
-        filename = filename.substr(0, filename.length() - 4);
+
+    // Remove common library extensions
+    std::string ext = std::filesystem::path(filename).extension().string();
+    if (ext == ".cpp" || ext == ".so" || ext == ".dll") {
+        filename = filename.substr(0, filename.length() - ext.length());
     }
-
-    // Extract the class name (everything after the last underscore if it exists)
-    size_t last_underscore = filename.find_last_of('_');
-    if (last_underscore != std::string::npos) {
-        filename = filename.substr(0, last_underscore);
+
+    // Remove optional "lib" prefix
+    if (filename.rfind("lib", 0) == 0) {
+        filename = filename.substr(3);
     }
-
+
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
-
+
     // Create a new loader for this library
     auto loader = std::make_unique<DynamicLibraryLoader>();
-
-    // Convert library path to full path with correct extension
-    std::string full_path;
+
+    // Use provided path directly if it already points to a shared library
+    std::string full_path = library_path;
+    if (library_path.length() >= 4 && library_path.substr(library_path.length() - 4) == ".cpp") {
 #ifdef _WIN32
-    // Map requested cpp to the corresponding DLL we build
-    if (library_path.find("AggressiveTankAI_") != std::string::npos) {
-        full_path = "cmake-build-debug/bin/libAlgorithm_Aggressive_212934582_323964676.dll";
-    } else if (library_path.find("TankAlgorithm_Simple_") != std::string::npos) {
-        full_path = "cmake-build-debug/bin/libAlgorithm_Simple_212934582_323964676.dll";
-    } else {
-        full_path = "cmake-build-debug/bin/libAlgorithm_Simple_212934582_323964676.dll"; // default
-    }
+        if (library_path.find("Aggressive") != std::string::npos) {
+            full_path = "lib/libAlgorithm_Aggressive_212934582_323964676.dll";
+        } else {
+            full_path = "lib/libAlgorithm_Simple_212934582_323964676.dll";
+        }
 #else
-    if (library_path.find("AggressiveTankAI_") != std::string::npos) {
-        full_path = "cmake-build-debug/lib/libAlgorithm_Aggressive_212934582_323964676.so";
-    } else if (library_path.find("TankAlgorithm_Simple_") != std::string::npos) {
-        full_path = "cmake-build-debug/lib/libAlgorithm_Simple_212934582_323964676.so";
-    } else {
-        full_path = "cmake-build-debug/lib/libAlgorithm_Simple_212934582_323964676.so";
-    }
+        if (library_path.find("Aggressive") != std::string::npos) {
+            full_path = "lib/libAlgorithm_Aggressive_212934582_323964676.so";
+        } else {
+            full_path = "lib/libAlgorithm_Simple_212934582_323964676.so";
+        }
 #endif
+    }
+
+    // Let PlayerRegistration know the normalized library name via env variable
+    std::string alg_name = getLibraryName(full_path);
+    setenv("CURRENT_ALG_LIB_NAME", alg_name.c_str(), 1);

-    // Load the library
     if (!loader->loadLibrary(full_path)) {
+        unsetenv("CURRENT_ALG_LIB_NAME");
         std::cerr << "Failed to load library: " << full_path << std::endl;
         std::cerr << "Error: " << loader->getLastError() << std::endl;
         return false;
     }
-
-    // Get the registration function that accepts our registrar pointer
-    typedef void (*RegisterFunction)(AlgorithmRegistrar*);
-    const char* symbol_name = (full_path.find("Aggressive") != std::string::npos)
-        ? "register_algorithms_aggressive_212934582_323964676"
-        : "register_algorithms_simple_212934582_323964676";
-    RegisterFunction register_func = reinterpret_cast<RegisterFunction>(loader->getFunction(symbol_name));
-
-    if (!register_func) {
-        std::cerr << "Failed to find initialization function in: " << full_path << std::endl;
-        std::cerr << "Error: " << loader->getLastError() << std::endl;
-        return false;
-    }
-
-    // Call the registration function with the host registrar
-    auto& host_registrar = AlgorithmRegistrar::getAlgorithmRegistrar();
-    register_func(&host_registrar);
-    std::cout << "Successfully loaded and initialized: " << full_path << std::endl;
-
-    // Keep the loader alive so function pointers remain valid
+
+    std::cout << "Successfully loaded: " << full_path << std::endl;
     loaded_algorithm_libraries_.push_back(std::move(loader));
-
+    // PlayerRegistration already consumed and unset env variable
     return true;
 }

 bool Simulator::loadGameManagerLibrary(const std::string& library_path) {
     std::cout << "Loading GameManager library: " << library_path << std::endl;
-
+
     auto loader = std::make_unique<DynamicLibraryLoader>();
-
-    // Convert library path to full path with correct extension
-    std::string full_path;
-#ifdef _WIN32
-    // For Windows: .dll files are in bin/ directory
+
+    // Convert .cpp input to corresponding shared library if needed
+    std::string full_path = library_path;
     if (library_path.length() >= 4 && library_path.substr(library_path.length() - 4) == ".cpp") {
-        // Use the GameManager shared library we created
-        full_path = "cmake-build-debug/bin/libGameManager_212934582_323964676.dll";
-    } else {
-        full_path = library_path;
-    }
+#ifdef _WIN32
+        full_path = "lib/libGameManager_212934582_323964676.dll";
 #else
-    // For Linux: .so files are in lib/ directory
-    if (library_path.length() >= 4 && library_path.substr(library_path.length() - 4) == ".cpp") {
-        full_path = "cmake-build-debug/lib/libGameManager_212934582_323964676.so";
-    } else {
-        full_path = library_path;
-    }
+        full_path = "lib/libGameManager_212934582_323964676.so";
 #endif
+    }

     if (!loader->loadLibrary(full_path)) {
         std::cerr << "Failed to load GameManager library: " << full_path << std::endl;
         std::cerr << "Error: " << loader->getLastError() << std::endl;
         return false;
     }
-
-    // Get the initialization function
-    typedef void (*InitFunction)();
-    InitFunction init_func = reinterpret_cast<InitFunction>(
-        loader->getFunction("initialize_gamemanager_212934582_323964676"));
-
-    if (!init_func) {
-        std::cerr << "Failed to find GameManager initialization function in: " << full_path << std::endl;
-        std::cerr << "Error: " << loader->getLastError() << std::endl;
-        return false;
-    }
-
-    init_func();
-    std::cout << "Successfully loaded and initialized GameManager: " << full_path << std::endl;
-
-    // Keep the loader alive so any GM symbols remain valid
+
+    std::cout << "Successfully loaded GameManager: " << full_path << std::endl;
     loaded_gamemanager_libraries_.push_back(std::move(loader));
-
     return true;
 }
