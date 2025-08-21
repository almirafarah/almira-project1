// MyBattleInfo.h - HW2 BattleInfo adapter for HW3
#ifndef USERCOMMON_MYBATTLEINFO_212934582_323964676_H
#define USERCOMMON_MYBATTLEINFO_212934582_323964676_H

#include "../common/BattleInfo.h"
#include <vector>
#include <string>
#include <cstddef>

namespace UserCommon_212934582_323964676 {

// A concrete BattleInfo carrying all the data your TankAlgorithm needs (from HW2)
struct MyBattleInfo : public BattleInfo {
    size_t rows;                        // Map height
    size_t cols;                        // Map width
    std::vector<std::string> board;     // Full snapshot of the map
    size_t x;                           // This tank's row
    size_t y;                           // This tank's col
    int direction;                      // This tank's facing (0-7, will be cast to Direction when needed)
    size_t shells_remaining;            // How many shells it has

    // Construct with everything at once
    MyBattleInfo(size_t rows_,
                 size_t cols_,
                 std::vector<std::string> board_,
                 size_t x_,
                 size_t y_,
                 int dir_,
                 size_t shells_)
      : rows(rows_)
      , cols(cols_)
      , board(std::move(board_))
      , x(x_)
      , y(y_)
      , direction(dir_)
      , shells_remaining(shells_)
    {}
};

} // namespace UserCommon_212934582_323964676

#endif // USERCOMMON_MYBATTLEINFO_212934582_323964676_H
