#pragma once

#include "../common/AbstractGameManager.h"
#include "../common/ActionRequest.h"
#include "../common/Player.h"
#include "../common/TankAlgorithm.h"
#include "../common/SatelliteView.h"
#include "../common/GameResult.h"
#include "../common/GameManagerRegistration.h" // for REGISTER_GAME_MANAGER
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

namespace GameManager_322837089_212141253 {

class GameManager : public AbstractGameManager {
public:
    explicit GameManager(bool verbose);
    ~GameManager() override = default;

    GameResult run(size_t map_width, size_t map_height,
                   const SatelliteView& map, std::string map_name,
                   size_t max_steps, size_t num_shells,
                   Player& player1, std::string name1,
                   Player& player2, std::string name2,
                   TankAlgorithmFactory player1_tank_algo_factory,
                   TankAlgorithmFactory player2_tank_algo_factory) override;

private:
    // ---------- helpers / state ----------
    struct Bullet {
        int x, y;
        int prevX, prevY;
        int dx, dy;
        bool live{true};
    };
    struct Position {
        int x, y;
        bool operator==(const Position& o) const { return x==o.x && y==o.y; }
    };
    struct TankState {
        int player{};
        int index{};
        int x{};
        int y{};
        int orientation{};     // 0..7
        int shells{};
        bool alive{true};
        int shootCooldown{0};
        int backwardWait{0};
        bool lastActionWasBackward{false};
        std::unique_ptr<TankAlgorithm> algorithm;
    };

    // The live, updating view we give to players when they ask for info
    class SatelliteViewImpl : public SatelliteView {
    public:
        SatelliteViewImpl(const GameManager& gmRef,
                          int reqTankIndex, int asking_player);
        char getObjectAt(size_t x, size_t y) const override;
    private:
        const GameManager& gm;
        int requestingTankIndex;
        int asking_player;
    };

    // A frozen snapshot to return inside GameResult at the end
    class SnapshotSatelliteView : public SatelliteView {
    public:
        explicit SnapshotSatelliteView(std::vector<std::string> grid) : grid_(std::move(grid)) {}
        char getObjectAt(size_t x, size_t y) const override {
            if (y >= grid_.size()) return '&';
            if (x >= grid_[y].size()) return '&';
            return grid_[y][x];
        }
    private:
        std::vector<std::string> grid_;
    };

    // ---------- core loop pieces ----------
    void resolveCollisions(std::vector<Position>& prevTankPos);
    bool occupied(int x, int y) const;

    // ---------- fields ----------
    bool verbose_{false};
    const int rounds_without_shells_limit{40};

    // inputs for this run
    int rows{0}, cols{0}, maxSteps{0}, initialShells{0};

    // dynamic state
    std::vector<std::string> board; // updated walls/mines
    std::vector<TankState>   tanks;
    std::vector<Bullet>      shells;
    int rounds_without_shells{0};

    Player* p1_{nullptr};
    Player* p2_{nullptr};

    // per-player algorithm factories
    TankAlgorithmFactory taf1_;
    TankAlgorithmFactory taf2_;
};

}

