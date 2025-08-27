#pragma once
#include <functional>
#include <memory>
#include "TankAlgorithm.h"
#include "SatelliteView.h"

class Player {
public:
    virtual ~Player() {}
    virtual void updateTankWithBattleInfo(
        TankAlgorithm& tank, SatelliteView& satellite_view) = 0;
};

// A3: factories are function aliases, not abstract classes
using PlayerFactory =
    std::function<std::unique_ptr<Player>(
        int player_index, size_t x, size_t y, size_t max_steps, size_t num_shells)>;
