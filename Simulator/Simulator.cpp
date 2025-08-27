#include "Simulator.h"

#include "../common/AbstractGameManager.h"
#include "../common/Player.h"
#include "../common/TankAlgorithm.h"
#include "../common/SatelliteView.h"
#include "../common/GameResult.h"

#include "AlgorithmRegistrar.h"
#include "GameManagerRegistrar.h"
#include "DynamicLibraryLoader.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <charconv>

namespace {
// Parses a positive integer from an arbitrary string using std::from_chars.
// Returns 0 on failure.
size_t parsePositiveNumber(const std::string &s) {
    std::string num;
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            num.push_back(c);
        }
    }
    if (num.empty()) return 0;
    size_t value = 0;
    auto [ptr, ec] = std::from_chars(num.data(), num.data() + num.size(), value);
    if (ec != std::errc()) return 0;
    return value;
}

// Print a brief usage line (for invalid inputs like no maps/algorithms)
void printUsage() {
    std::cerr
        << "Usage:\n"
        << "  simulator -comparative game_map=<path> game_managers_folder=<folder> "
           "algorithm1=<path_to_so> algorithm2=<path_to_so> [-threads=N] [-verbose]\n"
        << "  simulator -competition game_maps_folder=<folder> game_manager=<path_to_so> "
           "algorithms_folder=<folder> [-threads=N] [-verbose]\n";
}
} // namespace

// ---- platform lib extension (Windows: .dll, Unix: .so) ----
static std::mutex algorithm_load_mutex;
#ifdef _WIN32
static const std::string LIB_EXTENSION = ".dll";
#else
static const std::string LIB_EXTENSION = ".so";
#endif

// ------------------------------------------------------------
// Singleton access and registration helpers (as per patch)
// ------------------------------------------------------------
Simulator& Simulator::getInstance() {
    static Simulator instance;
    return instance;
}

void Simulator::registerGameManagerFactory(std::function<std::unique_ptr<AbstractGameManager>(bool)> factory) {
    Simulator::getInstance().gmFactories_.push_back(std::move(factory));
}

void Simulator::registerPlayerFactory(PlayerFactory factory) {
    Simulator::getInstance().playerFactories_.push_back(std::move(factory));
}

void Simulator::registerTankAlgorithmFactory(TankAlgorithmFactory factory) {
    Simulator::getInstance().tankFactories_.push_back(std::move(factory));
}

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
        if (worker.joinable()) worker.join();
    }
    workers_.clear();
}

// ------------------------------------------------------------
// Thread pool implementation
// ------------------------------------------------------------
void Simulator::initializeThreadPool(int num_threads, size_t /*total_tasks*/) {
    stop_workers_ = false;
    for (int i = 0; i < std::max(1, num_threads); ++i) {
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
    if (workers_.empty()) {
        // no pool → run inline
        SimulatorGameResult result = executeGame(task);
        std::lock_guard<std::mutex> lock(results_mutex_);
        game_results_.push_back(std::move(result));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(task);
    }
    condition_.notify_one();
}

void Simulator::waitForAllTasks() {
    if (workers_.empty()) return;

    // wait until queue drains
    for (;;) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (task_queue_.empty()) break;
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_workers_ = true;
    }
    condition_.notify_all();

    for (auto &worker : workers_) {
        if (worker.joinable()) worker.join();
    }
    workers_.clear();
    stop_workers_ = false;
}

// ------------------------------------------------------------
// Library loading helpers (registrar pattern)
// ------------------------------------------------------------
bool Simulator::loadAlgorithmLibrary(const std::string &library_path) {
    auto loader = std::make_unique<DynamicLibraryLoader>();
    std::string base = std::filesystem::path(library_path).filename().string();

    // Mark a new expected algorithm entry (so static initializers can fill it)
    AlgorithmRegistrar::get().createAlgorithmFactoryEntry(base);

    if (!loader->loadLibrary(library_path)) {
        std::cerr << "Failed to load algorithm library: " << library_path << "\n"
                  << "Error: " << loader->getLastError() << std::endl;
        AlgorithmRegistrar::get().removeLast();
        return false;
    }
    try {
        AlgorithmRegistrar::get().validateLastRegistration();
    } catch (AlgorithmRegistrar::BadRegistrationException&) {
        std::cerr << "Error: Algorithm file '" << base
                  << "' did not register required classes.\n";
        AlgorithmRegistrar::get().removeLast();
        return false;
    }

    loaded_algorithm_libraries_.push_back(std::move(loader));
    return true;
}

bool Simulator::loadGameManagerLibrary(const std::string &library_path) {
    auto loader = std::make_unique<DynamicLibraryLoader>();
    std::string base = std::filesystem::path(library_path).filename().string();

    GameManagerRegistrar::get().createGameManagerEntry(base);
    if (!loader->loadLibrary(library_path)) {
        std::cerr << "Failed to load GameManager library: " << library_path << "\n"
                  << "Error: " << loader->getLastError() << std::endl;
        GameManagerRegistrar::get().removeLast();
        return false;
    }
    try {
        GameManagerRegistrar::get().validateLastRegistration();
    } catch (GameManagerRegistrar::BadRegistrationException&) {
        std::cerr << "Error: GameManager file '" << base
                  << "' did not register a GameManager class.\n";
        GameManagerRegistrar::get().removeLast();
        return false;
    }

    loaded_gamemanager_libraries_.push_back(std::move(loader));
    return true;
}

// ------------------------------------------------------------
// Game execution
// ------------------------------------------------------------
SimulatorGameResult Simulator::executeGame(const GameTask &task) {
    // Create map view (do this first; on failure return empty result)
    auto map_view = createMapFromFile(task.map_path, task.map_width, task.map_height);
    if (!map_view) {
        std::cerr << "Failed to create map from file: " << task.map_path << std::endl;
        return {};
    }

    SimulatorGameResult result;

    // Store metadata
    result.game_manager_file = task.game_manager_path;
    result.algorithm1_file   = task.algorithm1_path;
    result.algorithm2_file   = task.algorithm2_path;
    result.map_name          = task.map_name;
    result.map_path          = task.map_path;
    result.map_width         = task.map_width;
    result.map_height        = task.map_height;

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

    // Load and register algorithms (thread-safe)
    std::unique_ptr<Player> player1;
    std::unique_ptr<Player> player2;
    TankAlgorithmFactory tank_algo_factory1;
    TankAlgorithmFactory tank_algo_factory2;
    {
        std::lock_guard<std::mutex> lock(algorithm_load_mutex);
        auto &registrar = AlgorithmRegistrar::get();
        registrar.clear();

        // Unload any previously loaded algos to force re-registration on next load
        for (auto &loader : loaded_algorithm_libraries_) {
            if (loader) loader->unload();
        }
        loaded_algorithm_libraries_.clear();
        // Clear after unloading
        registrar.clear();
        playerFactories_.clear();
        tankFactories_.clear();

        if (!loadAlgorithmLibrary(task.algorithm1_path) ||
            !loadAlgorithmLibrary(task.algorithm2_path)) {
            std::cerr << "Failed to load algorithm libraries" << std::endl;
            return result;
        }

        if (registrar.count() < 2) {
            std::cerr << "Algorithm registration incomplete. Expected 2, got "
                      << registrar.count() << std::endl;
            std::cerr << "Algorithm 1 path: " << task.algorithm1_path << std::endl;
            std::cerr << "Algorithm 2 path: " << task.algorithm2_path << std::endl;
            return result;
        }

        auto it = registrar.begin();
        auto algo1 = *it; ++it;
        auto algo2 = *it;

        result.algorithm1_name = algo1.name();
        result.algorithm2_name = algo2.name();

        player1 = algo1.createPlayer(1, p1_x, p1_y, task.max_steps, task.num_shells);
        player2 = algo2.createPlayer(2, p2_x, p2_y, task.max_steps, task.num_shells);

        tank_algo_factory1 = [algo1](int player_index, int tank_index) {
            return algo1.createTankAlgorithm(player_index, tank_index);
        };
        tank_algo_factory2 = [algo2](int player_index, int tank_index) {
            return algo2.createTankAlgorithm(player_index, tank_index);
        };
    }

    // Load GameManager (fresh each time)
    for (auto &loader : loaded_gamemanager_libraries_) {
        if (loader) loader->unload();
    }
    loaded_gamemanager_libraries_.clear();
    gmFactories_.clear();
    GameManagerRegistrar::get().clear();

    if (!loadGameManagerLibrary(task.game_manager_path)) {
        std::cerr << "Failed to load game manager library" << std::endl;
        return result;
    }

    auto& gmReg = GameManagerRegistrar::get();
    if (gmFactories_.empty() || gmReg.count() == 0) {
        std::cerr << "Error: GameManager file '" << task.game_manager_path
                  << "' did not register a GameManager class.\n";
        return result;
    }

    auto itGm = gmReg.end();
    --itGm;
    result.game_manager_name = itGm->name();
    auto gm_factory = gmFactories_.back();
    auto game_manager = gm_factory(task.verbose);

    // RAII cleanup to avoid leaks across games
    auto cleanup = [&]() {
        player1.reset();
        player2.reset();
        game_manager.reset();

        for (auto &loader : loaded_algorithm_libraries_) {
            if (loader) loader->unload();
        }
        for (auto &loader : loaded_gamemanager_libraries_) {
            if (loader) loader->unload();
        }
        loaded_algorithm_libraries_.clear();
        loaded_gamemanager_libraries_.clear();

        playerFactories_.clear();
        tankFactories_.clear();
        gmFactories_.clear();

        AlgorithmRegistrar::get().clear();
        GameManagerRegistrar::get().clear();
    };

    GameResult game_result;
    try {
        game_result = game_manager->run(
            task.map_width, task.map_height,
            *map_view,
            task.max_steps, task.num_shells,
            *player1, *player2,
            tank_algo_factory1, tank_algo_factory2
        );
    } catch (...) {
        std::cerr << "GameManager::run threw an exception" << std::endl;
        cleanup();
        result.game_result = std::move(game_result);
        return result;
    }

    result.game_result = std::move(game_result);
    cleanup();
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
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    size_t start_idx = 0;
    // Detect header (name + MaxSteps, NumShells, Rows, Cols)
    if (lines.size() >= 5 &&
        lines[1].find("MaxSteps")  != std::string::npos &&
        lines[2].find("NumShells") != std::string::npos &&
        lines[3].find("Rows")      != std::string::npos &&
        lines[4].find("Cols")      != std::string::npos) {
        start_idx = 5;
        if (height == 0) height = parsePositiveNumber(lines[3]);
        if (width  == 0) width  = parsePositiveNumber(lines[4]);
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
        [[nodiscard]] char getObjectAt(size_t xCoord, size_t yCoord) const override {
            if (yCoord >= board_.size() || xCoord >= board_[yCoord].size()) return '&';
            return board_[yCoord][xCoord];
        }
        [[nodiscard]] std::unique_ptr<SatelliteView> clone() const override {
            return std::make_unique<FileSatelliteView>(*this);
        }
    private:
        std::vector<std::string> board_;
    };

    return std::make_unique<FileSatelliteView>(std::move(board));
}

// ------------------------------------------------------------
// Output helpers
// ------------------------------------------------------------
namespace {
    // Human readable message for comparative clustering (optional use)
    std::string resultMessage(const GameResult &gr) {
        switch (gr.reason) {
            case GameResult::ALL_TANKS_DEAD:
                if (gr.winner == 0)
                    return "Tie: all tanks destroyed";
                else
                    return "Player " + std::to_string(gr.winner) +
                           " won: all opponent tanks destroyed";
            case GameResult::MAX_STEPS:
                if (gr.winner == 0)
                    return "Tie: maximum rounds reached";
                else
                    return "Player " + std::to_string(gr.winner) +
                           " won: more tanks remaining after maximum rounds";
            case GameResult::ZERO_SHELLS:
                if (gr.winner == 0)
                    return "Tie: no shells remain";
                else
                    return "Player " + std::to_string(gr.winner) +
                           " won: opponent ran out of shells";
        }
        return "";
    }

    struct ResultKey {
        int winner;
        GameResult::Reason reason;
        bool operator<(const ResultKey &other) const {
            return std::tie(winner, reason) <
                   std::tie(other.winner, other.reason);
        }
    };
} // namespace

void Simulator::writeComparativeResults(const std::string &output_folder,
                                        const std::vector<SimulatorGameResult> &results) {
    if (results.empty()) return;

    // Group by identical (winner, reason)
    std::map<ResultKey, std::vector<const SimulatorGameResult*>> groups;
    for (const auto &res : results) {
        ResultKey key{res.game_result.winner, res.game_result.reason};
        groups[key].push_back(&res);
    }

    struct GroupData {
        std::vector<const SimulatorGameResult*> entries;
        ResultKey key;
    };
    std::vector<GroupData> ordered;
    for (auto &kv : groups) {
        ordered.push_back({kv.second, kv.first});
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const GroupData &a, const GroupData &b) {
                  return a.entries.size() > b.entries.size();
              });

    // Header
    std::ostringstream output;
    auto map_filename = std::filesystem::path(results.front().map_name).filename().string();
    auto alg1_filename = std::filesystem::path(results.front().algorithm1_file).filename().string();
    auto alg2_filename = std::filesystem::path(results.front().algorithm2_file).filename().string();
    output << "game_map=" << map_filename << '\n';
    output << "algorithm1=" << alg1_filename << '\n';
    output << "algorithm2=" << alg2_filename << '\n';
    output << '\n';

    for (size_t g = 0; g < ordered.size(); ++g) {
        auto &grp = ordered[g];
        // line e: comma separated list of game manager library names
        for (size_t i = 0; i < grp.entries.size(); ++i) {
            auto name = std::filesystem::path(grp.entries[i]->game_manager_file).filename().string();
            if (i) output << ',';
            output << name;
        }
        output << '\n';

        // line f: result message
        output << resultMessage(grp.entries.front()->game_result) << '\n';

        if (g + 1 < ordered.size()) output << '\n';
    }

    std::filesystem::create_directories(output_folder);
    std::string out_path = output_folder + "/comparative_results_" + generateTimestamp() + ".txt";
    std::ofstream out(out_path);
    if (!out.is_open()) {
        std::cerr << "Error: Could not create output file: " << out_path << std::endl;
        std::cout << output.str();
        return;
    }
    out << output.str();
}

void Simulator::writeCompetitionResults(const std::string &algorithms_folder,
                                        const std::string &game_maps_folder,
                                        const std::string &game_manager_file,
                                        const std::vector<SimulatorGameResult> &results) {
    // Aggregate scores: 3 for win, 1 for tie, 0 for loss
    std::unordered_map<std::string, int> scores;

    // Ensure all seen algos appear (even if all losses)
    auto ensureAlgo = [&](const std::string &path) {
        std::string name = getLibraryName(path);
        if (!scores.count(name)) scores[name] = 0;
        return name;
    };

    for (const auto &res : results) {
        std::string a1 = ensureAlgo(res.algorithm1_file);
        std::string a2 = ensureAlgo(res.algorithm2_file);

        switch (res.game_result.winner) {
            case 1: scores[a1] += 3; break;
            case 2: scores[a2] += 3; break;
            case 0: scores[a1] += 1; scores[a2] += 1; break;
            default: break;
        }
    }

    // Move to vector and sort by score desc
    std::vector<std::pair<std::string,int>> sorted;
    sorted.reserve(scores.size());
    for (auto &kv : scores) sorted.emplace_back(kv.first, kv.second);
    std::sort(sorted.begin(), sorted.end(),
              [](auto &a, auto &b){ return a.second > b.second; });

    // Build output per spec
    std::ostringstream output;
    output << "game_maps_folder=" << game_maps_folder << "\n";
    output << "game_manager=" << std::filesystem::path(game_manager_file).filename().string() << "\n\n";
    for (auto &p : sorted) {
        output << p.first << " " << p.second << "\n";
    }

    // Write to file under algorithms_folder
    std::filesystem::create_directories(algorithms_folder);
    std::string filename = algorithms_folder + "/competition_" + generateTimestamp() + ".txt";
    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot create competition results file \"" << filename
                  << "\". Writing results to standard output.\n";
        std::cout << output.str();
        return;
    }
    out << output.str();
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
    try {
        for (const auto &entry : std::filesystem::directory_iterator(folder)) {
            if (entry.is_regular_file() && entry.path().extension() == extension) {
                files.push_back(entry.path().filename().string());
            }
        }
    } catch (const std::exception &ex) {
        std::cerr << "Error reading folder " << folder << ": " << ex.what() << std::endl;
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

    // collect game manager libraries (*.dll/.so) whose name contains "GameManager"
    auto gm_files = getFilesInFolder(game_managers_folder, LIB_EXTENSION);
    gm_files.erase(std::remove_if(gm_files.begin(), gm_files.end(),
                                  [](const std::string &f) {
                                      return f.find("GameManager") == std::string::npos;
                                  }),
                   gm_files.end());
    if (gm_files.empty()) {
        std::cerr << "No GameManager libraries found in folder: "
                  << game_managers_folder << std::endl;
        printUsage();
        return false;
    }

    // parse map metadata (optional header)
    size_t width = 0, height = 0, max_steps = 50, num_shells = 10;
    {
        std::ifstream map_file(game_map);
        if (map_file.is_open()) {
            std::vector<std::string> lines;
            std::string l;
            while (std::getline(map_file, l)) lines.push_back(l);
            if (lines.size() >= 5) {
                max_steps  = parsePositiveNumber(lines[1]);
                num_shells = parsePositiveNumber(lines[2]);
                height     = parsePositiveNumber(lines[3]);
                width      = parsePositiveNumber(lines[4]);
            }
        }
    }

    std::string map_name = std::filesystem::path(game_map).filename().string();

    initializeThreadPool(std::max(1, num_threads), gm_files.size());
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
        printUsage();
        return false;
    }

    // algorithms (*.dll/.so) whose name contains "Algorithm"
    auto algo_files = getFilesInFolder(algorithms_folder, LIB_EXTENSION);
    algo_files.erase(std::remove_if(algo_files.begin(), algo_files.end(),
                                    [](const std::string &f) {
                                        return f.find("Algorithm") == std::string::npos;
                                    }),
                     algo_files.end());
    const size_t N = algo_files.size();
    if (N < 2) {
        std::cerr << "Need at least two algorithm libraries in folder: "
                  << algorithms_folder << std::endl;
        printUsage();
        return false;
    }

    const size_t total_tasks = map_files.size() * (N * (N - 1) / 2);
    if (verbose_) {
        std::cout << "Total tasks to be created: " << total_tasks << std::endl;
    }

    initializeThreadPool(std::max(1, num_threads), total_tasks);

    // Round-robin tournament-style pairing that varies by map index k
    for (size_t k = 0; k < map_files.size(); ++k) {
        const auto &map_file = map_files[k];
        std::string map_path = game_maps_folder + "/" + map_file;
        std::string map_name = map_file;

        if (verbose_) {
            std::cout << "\nMap " << k << " (" << map_file << ") pairings:" << std::endl;
        }

        // parse map metadata (optional header)
        size_t width = 0, height = 0, max_steps = 50, num_shells = 10;
        std::ifstream map_file_stream(map_path);
        if (map_file_stream.is_open()) {
            std::vector<std::string> lines;
            std::string l;
            while (std::getline(map_file_stream, l)) lines.push_back(l);
            if (lines.size() >= 5) {
                max_steps  = parsePositiveNumber(lines[1]);
                num_shells = parsePositiveNumber(lines[2]);
                height     = parsePositiveNumber(lines[3]);
                width      = parsePositiveNumber(lines[4]);
            }
        }

        for (size_t i = 0; i < N; ++i) {
            // Tournament formula: j = (i + 1 + k % (N - 1)) % N
            size_t j = (i + 1 + (N > 1 ? (k % (N - 1)) : 0)) % N;

            // Skip duplicates / self-play
            if (i >= j) continue;

            if (verbose_) {
                std::cout << "  Algorithm " << i << " (" << algo_files[i] << ")"
                          << " vs "
                          << "Algorithm " << j << " (" << algo_files[j] << ")"
                          << std::endl;
            }

            // Build absolute paths
            std::string alg1_path = algorithms_folder + "/" + algo_files[i];
            std::string alg2_path = algorithms_folder + "/" + algo_files[j];

            GameTask task(game_manager, alg1_path, alg2_path, map_path,
                          map_name, width, height, max_steps, num_shells, verbose);
            submitTask(task);
        }
    }

    waitForAllTasks();
    writeCompetitionResults(algorithms_folder, game_maps_folder, game_manager, game_results_);
    return true;
}
