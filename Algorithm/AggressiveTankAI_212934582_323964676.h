#ifndef AGGRESSIVETANKAI_212934582_323964676_H
#define AGGRESSIVETANKAI_212934582_323964676_H

#include "../common/TankAlgorithm.h"
#include "../common/BattleInfo.h"
#include "../common/ActionRequest.h"
#include "../UserCommon/Utils_212934582_323964676.h"  // For Direction and DirectionUtils
#include <vector>
#include <string>
#include <queue>
#include <utility>
#include <climits>

namespace Algorithm_212934582_323964676 {

// Import Direction utilities from UserCommon
using UserCommon_212934582_323964676::Direction;
using UserCommon_212934582_323964676::DirectionUtils;

class AggressiveTankAI_212934582_323964676 : public TankAlgorithm {
public:
    AggressiveTankAI_212934582_323964676(int playerIndex, int tankIndex);
    ~AggressiveTankAI_212934582_323964676() noexcept override = default;

    // Return the next action for this tank
    ActionRequest getAction() override;

    // Receive fresh battle information at the start of a round
    void updateBattleInfo(BattleInfo& info) override;

private:
    int                            _playerIndex;      // 1 for Player 1
    int                            _tankIndex;        // zero-based index of this tank
    Direction                      _direction;        // current facing
    size_t                         _x, _y;            // current coordinates: _x=row, _y=column (HW2 convention)
    size_t                         _rows, _cols;      // map dimensions
    size_t                         _shellsRemaining;  // how many shells left
    bool                           _gotBattleInfo;    // whether we've received fresh info

    std::vector<std::string>       _board;            // last round snapshot of entire map
    std::vector<std::pair<int,int>> _enemyPositions;   // coordinates of all alive enemies
    std::vector<std::pair<int,int>> _currentPath;      // cached BFS path: sequence of (r,c) from next move to shooting cell

    // Scan _board to populate _enemyPositions.
    void findEnemies();

    // Return true if cell (r,c) is not passable (wall '#', mine '@', or any tank '1'/'2').
    bool isCellBlocked(int r, int c) const;

    bool hasLineOfSight(int fromR, int fromC, int toR, int toC) const;

    Direction directionBetween(int fromR, int fromC, int toR, int toC) const;

    bool isValidDiagonal(int r, int c, int nr, int nc) const;

    // Run a BFS from (_x,_y) to find the nearest cell from which we can see any enemy.
    // If found, populate _currentPath with the sequence of coordinates (excluding (_x,_y)):
    //   step 0 = first move‐to cell, …, last move = final shooting cell.
    // Otherwise, leave _currentPath empty.
    void computePathToNearestEnemy();

public:
    void setDirection(Direction d) { _direction = d; }
    size_t getX() const { return _x; }
    size_t getY() const { return _y; }
    Direction getDirection() const { return _direction; }
    size_t getShellsRemaining() const { return _shellsRemaining; }
};

} // namespace Algorithm_212934582_323964676

#endif // AGGRESSIVETANKAI_212934582_323964676_H
