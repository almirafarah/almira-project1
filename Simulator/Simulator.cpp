#include "Simulator.h"
#include "../common/AbstractGameManager.h"
#include "../common/Player.h"
#include "../common/TankAlgorithm.h"
#include "../common/SatelliteView.h"
#include "../common/GameResult.h"
#include "../common/GameManagerRegistration.h"
#include "AlgorithmRegistrar.h"
#include "DynamicLibraryLoader.h"
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <tuple>
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
#include <charconv>
#include <string>
#include <map>
#include <tuple>
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
} // namespace

// ---- platform lib extension (Windows: .dll, Unix: .so) ----
static std::mutex algorithm_load_mutex;
#ifdef _WIN32
static const std::string LIB_EXTENSION = ".dll";
#else
static const std::string LIB_EXTENSION = ".so";
#endif

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
// Library loading helpers
// ------------------------------------------------------------
bool Simulator::loadAlgorithmLibrary(const std::string &library_path) {
    auto loader = std::make_unique<DynamicLibraryLoader>();
    if (!loader->loadLibrary(library_path)) {
        std::cerr << "Failed to load algorithm library: " << library_path << "\n"
                  << "Error: " << loader->getLastError() << std::endl;
        return false;
    }

    // Optional registration hooks exported by some DLLs/SOs
    using ForceInitFunc = int (*)();
    if (auto func = reinterpret_cast<ForceInitFunc>(loader->getFunction("force_registration_initialization"))) {
        func();
    } else {
        using InitFunc = void (*)();
        if (auto init = reinterpret_cast<InitFunc>(loader->getFunction("initialize_algorithm"))) {
            init();
        }
    }

    loaded_algorithm_libraries_.push_back(std::move(loader));
    return true;
}

bool Simulator::loadGameManagerLibrary(const std::string &library_path) {
    auto loader = std::make_unique<DynamicLibraryLoader>();
    if (!loader->loadLibrary(library_path)) {
        std::cerr << "Failed to load GameManager library: " << library_path << "\n"
                  << "Error: " << loader->getLastError() << std::endl;
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
        auto &registrar = AlgorithmRegistrar::getAlgorithmRegistrar();
        registrar.clear();

        // Unload any previously loaded algos to force re-registration on next load
        for (auto &loader : loaded_algorithm_libraries_) {
            if (loader) loader->unload();
        }
        loaded_algorithm_libraries_.clear();

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
// Output writers

// ------------------------------------------------------------
namespace {
    // Convert final game state into vector of lines for output
    std::vector<std::string> gameStateToLines(const GameResult &gr, size_t width, size_t height) {
        std::vector<std::string> lines;
        if (!gr.gameState) return lines;
        for (size_t y = 0; y < height; ++y) {
            std::string row;
            for (size_t x = 0; x < width; ++x) {
                row.push_back(gr.gameState->getObjectAt(x, y));
            }
            lines.push_back(row);
        }
        return lines;
    }

    // Create a human readable message for game result
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
        size_t rounds;
        std::string board;
        bool operator<(const ResultKey &other) const {
            return std::tie(winner, reason, rounds, board) <
                   std::tie(other.winner, other.reason, other.rounds, other.board);
        }
    };
} // namespace


void Simulator::writeComparativeResults(const std::string &output_folder,
                                        const std::vector<SimulatorGameResult> &results) {
       if (results.empty()) return;

    if (results.empty()) return;

    // Build groups of game managers by identical final result
    std::map<ResultKey, std::vector<const SimulatorGameResult*>> groups;
    for (const auto &res : results) {
        std::string board;
        auto lines = gameStateToLines(res.game_result, res.map_width, res.map_height);
        for (size_t i = 0; i < lines.size(); ++i) {
            board += lines[i];
            if (i + 1 < lines.size()) board += '\n';
        }
        ResultKey key{res.game_result.winner, res.game_result.reason,
                       res.game_result.rounds, board};
        groups[key].push_back(&res);
    }

    // Convert to vector and sort by group size (largest first)
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

    // Prepare header information
    std::ostringstream output;
    auto map_filename = std::filesystem::path(results.front().map_name).filename().string();
    auto alg1_filename = std::filesystem::path(results.front().algorithm1_file).filename().string();
    auto alg2_filename = std::filesystem::path(results.front().algorithm2_file).filename().string();
    output << "game_map=" << map_filename << '\n';
    output << "algorithm1=" << alg1_filename << '\n';
    output << "algorithm2=" << alg2_filename << '\n';
    output << '\n';

    // Write each group
    for (size_t g = 0; g < ordered.size(); ++g) {
        auto &grp = ordered[g];
        // line e: comma separated list of game manager names
        for (size_t i = 0; i < grp.entries.size(); ++i) {
            auto name = std::filesystem::path(grp.entries[i]->game_manager_file).filename().string();
            if (i) output << ',';
            output << name;
        }
        output << '\n';

        // line f: game result message
        output << resultMessage(grp.entries.front()->game_result) << '\n';

        // line g: round number
        output << grp.entries.front()->game_result.rounds << '\n';

        // line h: final map
        auto board_lines = gameStateToLines(grp.entries.front()->game_result,
                                            grp.entries.front()->map_width,
                                            grp.entries.front()->map_height);
        for (const auto &line : board_lines) {
            output << line << '\n';
        }

        if (g + 1 < ordered.size()) {
            output << '\n';
        }
    }


    // Determine output path
    std::filesystem::create_directories(output_folder);
    std::string out_path = output_folder;
    out_path += "/comparative_results_";
    out_path += generateTimestamp();
    out_path += ".txt";

    std::ofstream out(out_path);
    if (!out.is_open()) {
        std::cerr << "Error: Could not create output file: " << out_path << std::endl;
        std::cout << output.str();
        return;
    }
    out << output.str();
}

void Simulator::writeCompetitionResults(const std::string &output_folder,
                                        const std::string &game_maps_folder,
                                        const std::string &game_manager_file,
                                        const std::vector<SimulatorGameResult> &results) {
    if (results.empty()) return;

    // Accumulate scores per algorithm
    std::unordered_map<std::string, int> scores;
    for (const auto &res : results) {
        std::string alg1 = getLibraryName(res.algorithm1_file);
        std::string alg2 = getLibraryName(res.algorithm2_file);
        scores.try_emplace(alg1, 0);
        scores.try_emplace(alg2, 0);
        switch (res.game_result.winner) {
            case 1: scores[alg1] += 3; break;
            case 2: scores[alg2] += 3; break;
            default:
                scores[alg1] += 1;
                scores[alg2] += 1;
                break;
        }
    }

    // Sort by score descending
    std::vector<std::pair<std::string, int>> ordered(scores.begin(), scores.end());
    std::sort(ordered.begin(), ordered.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });
    std::ostringstream output;
    output << "game_maps_folder=" << game_maps_folder << '\n';
    output << "game_manager=" << std::filesystem::path(game_manager_file).filename().string() << '\n';
    output << '\n';
    for (const auto &p : ordered) {
        output << p.first << ' ' << p.second << '\n';
    }
    std::filesystem::create_directories(output_folder);

    std::string filename = output_folder + "/competition_" + generateTimestamp() + ".txt";
    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Error: Could not create output file: " << filename << std::endl;
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

    // collect game manager libraries (*.dll/.so)
    auto gm_files = getFilesInFolder(game_managers_folder, LIB_EXTENSION);
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
        std::string gm_path = game_managers_folder;
        gm_path += "/";
        gm_path += gm_file;
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

    // algorithms (*.dll/.so)
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
        return false;
    }

    // Precompute total number of games
    size_t total_tasks = 0;
    for (size_t k = 0; k < map_files.size(); ++k) {
        size_t off1 = 1 + k % (N - 1);
        size_t off2 = 1 + (k + 1) % (N - 1);
        std::set<std::pair<size_t, size_t>> pairs;
        for (size_t i = 0; i < N; ++i) {
            size_t j1 = (i + off1) % N;
            size_t j2 = (i + off2) % N;
            if (i != j1) pairs.insert({std::min(i, j1), std::max(i, j1)});
            if (i != j2) pairs.insert({std::min(i, j2), std::max(i, j2)});
        }
        total_tasks += pairs.size();
    }
    if (verbose_) {

        std::cout << "Total tasks to be created: " << total_tasks << std::endl;
    }

    initializeThreadPool(std::max(1, num_threads), total_tasks);
    // Schedule games for each map
    for (size_t k = 0; k < map_files.size(); ++k) {
        const auto &map_file = map_files[k];
        std::string map_path = game_maps_folder;
        map_path += "/";
        map_path += map_file;
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
        size_t off1 = 1 + k % (N - 1);
        size_t off2 = 1 + (k + 1) % (N - 1);
        std::set<std::pair<size_t, size_t>> pairs;
        for (size_t i = 0; i < N; ++i) {
            size_t j1 = (i + off1) % N;
            size_t j2 = (i + off2) % N;
            if (i != j1) pairs.insert({std::min(i, j1), std::max(i, j1)});
            if (i != j2) pairs.insert({std::min(i, j2), std::max(i, j2)});
        }
        for (const auto &p : pairs) {
            size_t i = p.first;
            size_t j = p.second;

            if (verbose_) {
                std::cout << "  Algorithm " << i << " (" << algo_files[i] << ")"
                          << " vs "
                << "Algorithm " << j << " (" << algo_files[j] << ")" << std::endl;
            }

            // Create task for this pairing
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
