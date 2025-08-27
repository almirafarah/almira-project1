#pragma once

#include "SatelliteView.h"
#include "Player.h"
#include "TankAlgorithm.h"
#include "GameResult.h"

class AbstractGameManager {
public:
    virtual ~AbstractGameManager() {}
    virtual GameResult run(
        size_t map_width, size_t map_height,
        SatelliteView& map,
        size_t max_steps, size_t num_shells,
        Player& player1, Player& player2,
        TankAlgorithmFactory player1_tank_algo_factory,
        TankAlgorithmFactory player2_tank_algo_factory) = 0;
};
