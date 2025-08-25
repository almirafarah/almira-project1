#include "Player_Aggressive_212934582_323964676.h"
#include "../common/BattleInfo.h"
#include "../UserCommon/MyBattleInfo_212934582_323964676.h"
#include "../common/PlayerRegistration.h"

namespace Algorithm_212934582_323964676 {

Player_Aggressive_212934582_323964676::Player_Aggressive_212934582_323964676(int player_index, size_t x, size_t y, size_t max_steps, size_t num_shells)
    : player_index_(player_index), x_(x), y_(y), max_steps_(max_steps), num_shells_(num_shells) {
}

void Player_Aggressive_212934582_323964676::updateTankWithBattleInfo(TankAlgorithm& tank_algo, SatelliteView& satellite_view) {
    using UserCommon_212934582_323964676::MyBattleInfo;

    // Determine map dimensions by probing the satellite view
    size_t width = 0;
    while (satellite_view.getObjectAt(width, 0) != '&') {
        ++width;
    }
    size_t height = 0;
    while (satellite_view.getObjectAt(0, height) != '&') {
        ++height;
    }

    // Reconstruct board and locate our tank (marked by '%')
    std::vector<std::string> board(height, std::string(width, ' '));
    size_t tank_row = 0, tank_col = 0;
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            char c = satellite_view.getObjectAt(x, y);
            if (c == '%') {
                tank_row = y;
                tank_col = x;
                c = static_cast<char>('0' + player_index_);
            }
            board[y][x] = c;
        }
    }

    // Build battle info with basic data; orientation and shells are not tracked
    MyBattleInfo info(height, width, board, tank_row, tank_col, 0, num_shells_);
    tank_algo.updateBattleInfo(info);
}

} // namespace Algorithm_212934582_323964676

// Register the aggressive player implementation with the simulator
using PlayerAggressive = Algorithm_212934582_323964676::Player_Aggressive_212934582_323964676;
REGISTER_PLAYER(PlayerAggressive)

