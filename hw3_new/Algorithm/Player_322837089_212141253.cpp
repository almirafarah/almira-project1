
#include <iostream>
#include <utility>
#include "../common/PlayerRegistration.h"

#include "Player_322837089_212141253.h"

using namespace Algorithm_322837089_212141253;

// -------------------------------------
// Player_322837089_212141253 implementation
// -------------------------------------
Player_322837089_212141253::Player_322837089_212141253(int player_index,
                   size_t width,
                   size_t height,
                   size_t max_steps,
                   size_t num_shells)
    : player_index_(player_index),
      width_(width),
      height_(height),
      max_steps_(max_steps),
      num_shells_(num_shells),
      size_known_(true) {}

// Called by GameManager when a tank requests GetBattleInfo
void Player_322837089_212141253::updateTankWithBattleInfo(TankAlgorithm& tank,
                                        SatelliteView& satellite_view)
{
    MyBattleInfo info;
    info.self_x = (size_t)-1;
    info.self_y = (size_t)-1;

    // Pre-size rows to avoid reallocations
    info.view.reserve(height_);

    for (size_t i = 0; i < height_; ++i) {
        std::string row;
        row.reserve(width_);

        for (size_t j = 0; j < width_; ++j) {
            char c = satellite_view.getObjectAt(j, i);
            row.push_back(c);

            if (c == '%') {           // our own tank location
                info.self_x = j;
                info.self_y = i;
            }
        }
        info.view.push_back(std::move(row));
    }

    if (info.self_x == (size_t)-1 || info.self_y == (size_t)-1) {
        for (size_t i = 0; i < height_ && info.self_x == (size_t)-1; ++i) {
            for (size_t j = 0; j < width_; ++j) {
                char c = satellite_view.getObjectAt(j, i);
                if ((player_index_ == 1 && c == '1') || (player_index_ == 2 && c == '2')) {
                    info.self_x = j;
                    info.self_y = i;
                    break;
                }
            }
        }
    }

    info.initial_shells = num_shells_;

    // Hand off to the tank algorithm
    tank.updateBattleInfo(info);
}

// -------------------------------------
// A3 factory helper
// -------------------------------------
PlayerFactory make_my_player_factory() {
    return [](int player_index, size_t x, size_t y,
              size_t max_steps, size_t num_shells) -> std::unique_ptr<Player>
    {
        if (player_index == 1) {
            return std::make_unique<PlayerOne>(player_index, x, y, max_steps, num_shells);
        } else {
            return std::make_unique<PlayerTwo>(player_index, x, y, max_steps, num_shells);
        }
    };
}

REGISTER_PLAYER(Player_322837089_212141253);
