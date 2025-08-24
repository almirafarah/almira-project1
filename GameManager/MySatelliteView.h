// MySatelliteView.h - HW2 SatelliteView adapter for HW3
#ifndef MYSATELLITEVIEW_H
#define MYSATELLITEVIEW_H

#include "../common/SatelliteView.h"
#include "GameManager_212934582_323964676.h"
#include <vector>
#include <set>
#include <utility>
#include <cstddef>

namespace GameManager_212934582_323964676 {

class MySatelliteView : public SatelliteView {
public:
    // Base constructor that takes (board, liveShells) with no highlight
    MySatelliteView(const std::vector<std::string>& board,
                    const std::vector<ShellState>& liveShells)
      : board_(board)
      , highlight_x_(static_cast<size_t>(-1))
      , highlight_y_(static_cast<size_t>(-1))
    {
        for (const auto& shell : liveShells) {
            shells_.insert({ shell.x, shell.y });
        }
    }

    // Constructor with highlight position
    MySatelliteView(const MySatelliteView& other,
                    size_t highlight_x,
                    size_t highlight_y)
      : board_(other.board_)
      , shells_(other.shells_)
      , highlight_x_(highlight_x)
      , highlight_y_(highlight_y)
    {}

    // Constructor: board + highlight + list of live shells
    MySatelliteView(const std::vector<std::string>& board,
                    size_t highlight_x,
                    size_t highlight_y,
                    const std::vector<ShellState>& liveShells)
      : board_(board)
      , highlight_x_(highlight_x)
      , highlight_y_(highlight_y)
    {
        // Populate shells from the vector<ShellState>
        for (const auto& shell : liveShells) {
            shells_.insert({ shell.x, shell.y });
        }
    }

    // Produce a copy of this view but marking (x,y) as '%'
    MySatelliteView withHighlight(size_t x, size_t y) const {
        return MySatelliteView(*this, x, y);
    }

    // SatelliteView interface implementation
    [[nodiscard]] char getObjectAt(size_t xCoord, size_t yCoord) const override {
        // 1) Outside board boundaries - board_[row][col], where row=y, col=x
        if (board_.empty() || yCoord >= board_.size() || board_[0].empty() || xCoord >= board_[0].size()) {
            return '&';
        }

        // 2) This tank's own spot marked as '%'
        if (xCoord == highlight_x_ && yCoord == highlight_y_) {
            return '%';
        }

        // 3) If a shell is flying over (x,y) marked as '*'
        if (shells_.count({xCoord, yCoord})) {
            return '*';
        }

        // 4) Otherwise return the static board character - board_[row][col] = board_[y][x]
        char c = board_[yCoord][xCoord];
        if (c == '#' || c == '@' || c == '1' || c == '2' || c == ' ') {
            return c;
        }
        return ' ';  // Unknown characters become empty space
    }

    // Clone method for copying
    [[nodiscard]] std::unique_ptr<SatelliteView> clone() const override {
        return std::make_unique<MySatelliteView>(*this);
    }

private:
    std::vector<std::string> board_;                    // The static game board
    std::set<std::pair<size_t, size_t>> shells_;        // Positions of flying shells
    size_t highlight_x_;                                // This tank's X position (marked as '%')
    size_t highlight_y_;                                // This tank's Y position (marked as '%')
};

} // namespace GameManager_212934582_323964676

#endif // MYSATELLITEVIEW_H
