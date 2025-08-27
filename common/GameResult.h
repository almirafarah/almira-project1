#pragma once

#include <vector>
#include <memory>
#include "SatelliteView.h"

struct GameResult {
    int winner; // 0 = tie

    enum Reason { ALL_TANKS_DEAD, MAX_STEPS, ZERO_SHELLS };
    Reason reason;

    // index 0 = player 1's remaining tanks, index 1 = player 2's remaining tanks, etc.
    std::vector<size_t> remaining_tanks;

    // snapshot of final game state at end of game
    std::unique_ptr<SatelliteView> gameState;

    // total number of rounds played
    size_t rounds;
};
