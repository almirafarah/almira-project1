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
#include <cctype>

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
// Game execution
// ------------------------------------------------------------
SimulatorGameResult Simulator::executeGame(const GameTask &task) {
    SimulatorGameResult result;

    // Store metadata
    result.game_manager_file = task.game_manager_path;
    result.algorithm1_file = task.algorithm1_path;
    result.algorithm2_file = task.algorithm2_path;
    result.map_name = task.map_name;
    result.map_path = task.map_path;
    result.map_width = task.map_width;
    result.map_height = task.map_height;

    // Create map view
    auto map_view = createMapFromFile(task.map_path, task.map_width, task.map_height);
    if (!map_view) {
        std::cerr << "Failed to create map from file: " << task.map_path << std::endl;
        return result;
    }

    // Find first tanks for players '1' and '2'
    size_t p1_x = 0, p1_y = 0, p2_x = 0, p2_y = 0;
    bool found_p1 = false, found_p2 = false;
    for (size_t y = 0; y < task.map_height; ++y) {
        for (size_t x = 0; x < task.map_width; ++x) {
            char c = map_view->getObjectAt(x, y);
            if (!found_p1 && c == '1') { p1_x = x; p1_y = y; found_p1 = true; }
            else if (!found_p2 && c == '2') { p2_x = x; p2_y = y; found_p2 = true; }
        }
    }

    // Load algorithms
    auto &registrar = AlgorithmRegistrar::getAlgorithmRegistrar();
    registrar.clear();
    loaded_algorithm_libraries_.clear();

    if (!loadAlgorithmLibrary(task.algorithm1_path) ||
        !loadAlgorithmLibrary(task.algorithm2_path)) {
        std::cerr << "Failed to load algorithm libraries" << std::endl;
        return result;
    }

    if (registrar.count() < 2) {
        std::cerr << "Algorithm registration incomplete" << std::endl;
        return result;
    }

    auto it = registrar.begin();
    const auto &algo1 = *it; ++it;
    const auto &algo2 = *it;

    result.algorithm1_name = algo1.name();
    result.algorithm2_name = algo2.name();

    // Create players
    auto player1 = algo1.createPlayer(1, p1_x, p1_y, task.max_steps, task.num_shells);
    auto player2 = algo2.createPlayer(2, p2_x, p2_y, task.max_steps, task.num_shells);

    // Tank algorithm factories for GameManager
    TankAlgorithmFactory tank_algo_factory1 = [&algo1](int player_index, int tank_index) {
        return algo1.createTankAlgorithm(player_index, tank_index);
    };
    TankAlgorithmFactory tank_algo_factory2 = [&algo2](int player_index, int tank_index) {
        return algo2.createTankAlgorithm(player_index, tank_index);
    };

    // Load GameManager
    if (!loadGameManagerLibrary(task.game_manager_path)) {
        std::cerr << "Failed to load game manager library" << std::endl;
        return result;
    }

    auto gm_names = getRegisteredGameManagers();
    if (gm_names.empty()) {
        std::cerr << "No GameManager registered" << std::endl;
        return result;
    }

    result.game_manager_name = gm_names.front();
    auto gm_factory = getGameManagerFactory(result.game_manager_name);
    if (!gm_factory) {
        std::cerr << "Failed to obtain GameManager factory" << std::endl;
        return result;
    }

    auto game_manager = gm_factory(task.verbose);

    // Run the game
    auto game_result = game_manager->run(
        task.map_width, task.map_height,
        *map_view, task.map_name,
        task.max_steps, task.num_shells,
        *player1, result.algorithm1_name,
        *player2, result.algorithm2_name,
        tank_algo_factory1, tank_algo_factory2
    );

    result.game_result = std::move(game_result);
    return result;
}

// ------------------------------------------------------------
// Map creation
// ------------------------------------------------------------
std::unique_ptr<SatelliteView> Simulator::createMapFromFile(const std::string &map_path,
                                                            size_t width, size_t height) {
    std::ifstream file(map_path);
    if (!file.is_open()) {
        std::cerr << "Cannot open map file: " << map_path << std::endl;
        return nullptr;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    size_t start_idx = 0;
    // Detect header (name + MaxSteps, NumShells, Rows, Cols)
    if (lines.size() >= 5 &&
        lines[1].find("MaxSteps") != std::string::npos &&
        lines[2].find("NumShells") != std::string::npos &&
        lines[3].find("Rows") != std::string::npos &&
        lines[4].find("Cols") != std::string::npos) {
        start_idx = 5;
        auto parseNum = [](const std::string &s) -> size_t {
            std::string num;
            for (char c : s) if (std::isdigit(static_cast<unsigned char>(c))) num.push_back(c);
            return num.empty() ? 0 : static_cast<size_t>(std::stoul(num));
        };
        if (height == 0) height = parseNum(lines[3]);
        if (width  == 0) width  = parseNum(lines[4]);
    }

    std::vector<std::string> board;
    for (size_t i = 0; i < height && start_idx + i < lines.size(); ++i) {
        std::string row = lines[start_idx + i];
        if (row.size() < width)      row += std::string(width - row.size(), ' ');
        else if (row.size() > width) row = row.substr(0, width);
        board.push_back(row);
    }

    class FileSatelliteView : public SatelliteView {
    public:
        explicit FileSatelliteView(std::vector<std::string> b) : board_(std::move(b)) {}
        virtual char getObjectAt(size_t x, size_t y) const override {
            if (y >= board_.size() || x >= board_[y].size()) return '&';
            return board_[y][x];
        }
        virtual std::unique_ptr<SatelliteView> clone() const override {
            return std::make_unique<FileSatelliteView>(*this);
        }
    private:
        std::vector<std::string> board_;
    };

    return std::make_unique<FileSatelliteView>(std::move(board));
}

// ------------------------------------------------------------
// Output writers
// ------------------------------------------------------------
void Simulator::writeComparativeResults(const std::string &output_folder,
                                        const std::vector<SimulatorGameResult> &results) {
    std::filesystem::create_directories(output_folder);
    std::ofstream out(output_folder + "/comparative_results.csv");
    if (!out.is_open()) return;
    out << "Algorithm1,Algorithm2,Winner\\n";
    for (const auto &res : results) {
        out << getLibraryName(res.algorithm1_file) << ','
            << getLibraryName(res.algorithm2_file) << ','
            << res.game_result.winner << "\\n";
    }
}

void Simulator::writeCompetitionResults(const std::string &output_folder,
                                        const std::vector<SimulatorGameResult> &results) {
    std::filesystem::create_directories(output_folder);
    std::string filename = output_folder + "/competition_" + generateTimestamp() + ".txt";
    std::ofstream out(filename);
    if (!out.is_open()) return;
    out << "Algorithm1,Algorithm2,Winner\\n";
    for (const auto &res : results) {
        out << getLibraryName(res.algorithm1_file) << ','
            << getLibraryName(res.algorithm2_file) << ','
            << res.game_result.winner << "\\n";
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
// High level run functions
// ------------------------------------------------------------
bool Simulator::runComparative(const std::string &game_map, const std::string &game_managers_folder,
                               const std::string &algorithm1, const std::string &algorithm2,
                               int num_threads, bool verbose) {
    verbose_ = verbose;
    game_results_.clear();

    initializeThreadPool(std::max<int>(1, num_threads), 1);

    // collect game manager libraries (*.so per diff)
    auto gm_files = getFilesInFolder(game_managers_folder, ".so");
    gm_files.erase(std::remove_if(gm_files.begin(), gm_files.end(),
                                  [](const std::string &f) {
                                      return f.find("GameManager") == std::string::npos;
                                  }),
                   gm_files.end());
    if (gm_files.empty()) {
        std::cerr << "No GameManager libraries found in folder: "
                  << game_managers_folder << std::endl;
        return false;
    }

    std::string map_name = std::filesystem::path(game_map).filename().string();

    // parse map metadata
    size_t width = 0, height = 0, max_steps = 50, num_shells = 10;
    {
        std::ifstream map_file(game_map);
        if (map_file.is_open()) {
            std::vector<std::string> lines;
            std::string l;
            while (std::getline(map_file, l)) lines.push_back(l);
            if (lines.size() >= 5) {
                auto parseNum = [](const std::string &s) -> size_t {
                    std::string num;
                    for (char c : s) if (std::isdigit(static_cast<unsigned char>(c))) num.push_back(c);
                    return num.empty() ? 0 : static_cast<size_t>(std::stoul(num));
                };
                max_steps  = parseNum(lines[1]);
                num_shells = parseNum(lines[2]);
                height     = parseNum(lines[3]);
                width      = parseNum(lines[4]);
            }
        }
    }

    for (const auto &gm_file : gm_files) {
        std::string gm_path = game_managers_folder + "/" + gm_file;
        GameTask task(gm_path, algorithm1, algorithm2, game_map,
                      map_name, width, height, max_steps, num_shells, verbose);
        submitTask(task);
    }

    waitForAllTasks();
    writeComparativeResults(game_managers_folder, game_results_);
    return true;
}

bool Simulator::runCompetition(const std::string &game_maps_folder, const std::string &game_manager,
                               const std::string &algorithms_folder, int num_threads, bool verbose) {
    verbose_ = verbose;
    game_results_.clear();

    // maps (*.txt)
    auto map_files = getFilesInFolder(game_maps_folder, ".txt");
    if (map_files.empty()) {
        std::cerr << "No map files found in folder: " << game_maps_folder << std::endl;
        return false;
    }

    // algorithms (*.so per diff)
    auto algo_files = getFilesInFolder(algorithms_folder, ".so");
    algo_files.erase(std::remove_if(algo_files.begin(), algo_files.end(),
                                    [](const std::string &f) {
                                        return f.find("Algorithm") == std::string::npos;
                                    }),
                     algo_files.end());
    if (algo_files.size() < 2) {
        std::cerr << "Need at least two algorithm libraries in folder: "
                  << algorithms_folder << std::endl;
        return false;
    }

    size_t total_tasks = map_files.size() * (algo_files.size() * (algo_files.size() - 1) / 2);
    initializeThreadPool(std::max<int>(1, num_threads), total_tasks);

    std::string gm_path = game_manager;

    for (const auto &map_file : map_files) {
        std::string map_path = game_maps_folder + "/" + map_file;
        std::string map_name = map_file;

        // parse map metadata
        size_t width = 0, height = 0, max_steps = 50, num_shells = 10;
        std::ifstream map_file_stream(map_path);
        if (map_file_stream.is_open()) {
            std::vector<std::string> lines;
            std::string l;
            while (std::getline(map_file_stream, l)) lines.push_back(l);
            if (lines.size() >= 5) {
                auto parseNum = [](const std::string &s) -> size_t {
                    std::string num;
                    for (char c : s) if (std::isdigit(static_cast<unsigned char>(c))) num.push_back(c);
                    return num.empty() ? 0 : static_cast<size_t>(std::stoul(num));
                };
                max_steps  = parseNum(lines[1]);
                num_shells = parseNum(lines[2]);
                height     = parseNum(lines[3]);
                width      = parseNum(lines[4]);
            }
        }

        for (size_t i = 0; i < algo_files.size(); ++i) {
            for (size_t j = i + 1; j < algo_files.size(); ++j) {
                std::string alg1_path = algorithms_folder + "/" + algo_files[i];
                std::string alg2_path = algorithms_folder + "/" + algo_files[j];
                GameTask task(gm_path, alg1_path, alg2_path, map_path,
                              map_name, width, height, max_steps, num_shells, verbose);
                submitTask(task);
            }
        }
    }

    waitForAllTasks();
    writeCompetitionResults(algorithms_folder, game_results_);
    return true;
}
