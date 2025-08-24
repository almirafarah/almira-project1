#include "Simulator.h"
#include "../common/AbstractGameManager.h"
#include "../common/Player.h"
#include "../common/TankAlgorithm.h"
#include "../common/SatelliteView.h"
#include "../common/GameResult.h"
#include "../common/GameManagerRegistration.h"
#include "AlgorithmRegistrar.h"
#include "DynamicLibraryLoader.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// ------------------------------------------------------------
// Construction / Destruction
// ------------------------------------------------------------
Simulator::Simulator() : stop_workers_(false), verbose_(false) {}

Simulator::~Simulator() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_workers_ = true;
    }
    condition_.notify_all();
    for (auto &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

// ------------------------------------------------------------
// Thread pool implementation
// ------------------------------------------------------------
void Simulator::initializeThreadPool(int num_threads, size_t /*total_tasks*/) {
    stop_workers_ = false;
    for (int i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&Simulator::workerThread, this);
    }
}

void Simulator::workerThread() {
    while (true) {
        GameTask task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] {
                return stop_workers_ || !task_queue_.empty();
            });
            if (stop_workers_ && task_queue_.empty()) {
                return;
            }
            task = task_queue_.front();
            task_queue_.pop();
        }
        SimulatorGameResult result = executeGame(task);
        {
            std::lock_guard<std::mutex> lock(results_mutex_);
            game_results_.push_back(std::move(result));
        }
        condition_.notify_all();
    }
}

void Simulator::submitTask(const GameTask &task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(task);
    }
    condition_.notify_one();
}

void Simulator::waitForAllTasks() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    condition_.wait(lock, [this] {
        return task_queue_.empty();
    });
    stop_workers_ = true;
    lock.unlock();
    condition_.notify_all();
    for (auto &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

// ------------------------------------------------------------
// Library loading helpers
// ------------------------------------------------------------
bool Simulator::loadAlgorithmLibrary(const std::string &library_path) {
    auto loader = std::make_unique<DynamicLibraryLoader>();
    if (!loader->loadLibrary(library_path)) {
        return false;
    }
    loaded_algorithm_libraries_.push_back(std::move(loader));
    return true;
}

bool Simulator::loadGameManagerLibrary(const std::string &library_path) {
    auto loader = std::make_unique<DynamicLibraryLoader>();
    if (!loader->loadLibrary(library_path)) {
        return false;
    }
    loaded_gamemanager_libraries_.push_back(std::move(loader));
    return true;
}

// ------------------------------------------------------------
// Game execution (stubbed)
// ------------------------------------------------------------
SimulatorGameResult Simulator::executeGame(const GameTask &task) {
    SimulatorGameResult result;
    result.game_manager_file = task.game_manager_path;
    result.algorithm1_file = task.algorithm1_path;
    result.algorithm2_file = task.algorithm2_path;
    result.map_name = task.map_name;
    result.map_path = task.map_path;
    result.map_width = task.map_width;
    result.map_height = task.map_height;
    return result;
}

std::unique_ptr<SatelliteView> Simulator::createMapFromFile(const std::string & /*map_path*/,
                                                            size_t /*width*/, size_t /*height*/) {
    return nullptr; // not implemented in this stub
}

// ------------------------------------------------------------
// Output writers
// ------------------------------------------------------------
void Simulator::writeComparativeResults(const std::string &output_folder,
                                        const std::vector<SimulatorGameResult> &results) {
    std::filesystem::create_directories(output_folder);
    std::ofstream out(output_folder + "/comparative_results.csv");
    if (!out.is_open()) return;
    out << "Algorithm1,Algorithm2,Winner\n";
    for (const auto &res : results) {
        out << getLibraryName(res.algorithm1_file) << ','
            << getLibraryName(res.algorithm2_file) << ','
            << res.game_result.winner << "\n";
    }
}

void Simulator::writeCompetitionResults(const std::string &output_folder,
                                        const std::vector<SimulatorGameResult> &results) {
    std::filesystem::create_directories(output_folder);
    std::ofstream out(output_folder + "/competition_results.csv");
    if (!out.is_open()) return;
    out << "Algorithm1,Algorithm2,Winner\n";
    for (const auto &res : results) {
        out << getLibraryName(res.algorithm1_file) << ','
            << getLibraryName(res.algorithm2_file) << ','
            << res.game_result.winner << "\n";
    }
}

// ------------------------------------------------------------
// Utility helpers
// ------------------------------------------------------------
std::string Simulator::getLibraryName(const std::string &path) {
    return std::filesystem::path(path).stem().string();
}

std::vector<std::string> Simulator::getFilesInFolder(const std::string &folder,
                                                     const std::string &extension) {
    std::vector<std::string> files;
    for (const auto &entry : std::filesystem::directory_iterator(folder)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            files.push_back(entry.path().filename().string());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string Simulator::generateTimestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t tt = system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

// ------------------------------------------------------------
// High level run functions (minimal stubs, per diff)
// ------------------------------------------------------------
bool Simulator::runComparative(const std::string &game_map, const std::string &game_managers_folder,
                               const std::string &algorithm1, const std::string &algorithm2,
                               int num_threads, bool verbose) {
    verbose_ = verbose;
    game_results_.clear();
    initializeThreadPool(std::max<int>(1, num_threads), 1);
    GameTask task("", algorithm1, algorithm2, game_map,
                  std::filesystem::path(game_map).filename().string(), 0, 0, 50, 10, verbose);
    submitTask(task);
    waitForAllTasks();
    writeComparativeResults(game_managers_folder, game_results_);
    return true;
}

bool Simulator::runCompetition(const std::string &game_maps_folder, const std::string &game_manager,
                               const std::string &algorithms_folder, int num_threads, bool verbose) {
    (void)game_maps_folder;
    (void)game_manager;
    (void)algorithms_folder;
    verbose_ = verbose;
    game_results_.clear();
    initializeThreadPool(std::max<int>(1, num_threads), 0);
    waitForAllTasks();
    writeCompetitionResults(".", game_results_);
    return true;
}
