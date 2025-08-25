#include "AggressiveTankAI_212934582_323964676.h"
#include "../UserCommon/MyBattleInfo_212934582_323964676.h"
#include "../common/TankAlgorithmRegistration.h"
#include <cctype>
#include <iostream>  // debug printing
#include <queue>
#include <tuple>
#include <optional>
#include <climits>

namespace Algorithm_212934582_323964676 {

// Import MyBattleInfo from UserCommon
using UserCommon_212934582_323964676::MyBattleInfo;

AggressiveTankAI_212934582_323964676::AggressiveTankAI_212934582_323964676(int playerIndex, int tankIndex)
    : _playerIndex(playerIndex)
    , _tankIndex(tankIndex)
    , _direction(Direction::LEFT)  // default
    , _x(0)
    , _y(0)
    , _rows(0)
    , _cols(0)
    , _shellsRemaining(0)
    , _gotBattleInfo(false)
{
}

// updateBattleInfo
void AggressiveTankAI_212934582_323964676::updateBattleInfo(BattleInfo& info) {
    // cast to our MyBattleInfo
    auto& bi = static_cast<MyBattleInfo&>(info);

    _rows            = bi.rows;
    _cols            = bi.cols;
    _board           = bi.board;        // copy entire map snapshot
    // HW2 convention: bi.x is row, bi.y is column
    _x               = bi.x;  // row
    _y               = bi.y;  // column
    _direction       = static_cast<Direction>(bi.direction);
    _shellsRemaining = bi.shells_remaining;
    _gotBattleInfo   = true;

    

    // rebuild the list of enemy coordinates
    findEnemies();
}

// helper: check whether from (r,c) facing 'd' you can shoot any enemy.
// returns true as soon as straight‐line ray from (r,c) in direction d
// hits a cell containing a digit (enemy != _playerIndex) without hitting a wall/ mine first.
static bool canShootFrom(
    const std::vector<std::string>& board,
    const std::vector<std::pair<int,int>>& enemyPositions,
    int rows,
    int cols,
    int r,
    int c,
    Direction d,
    int myPlayerIndex
) {
    (void)enemyPositions;
    auto [dr, dc] = DirectionUtils::toVector(d);
    
    for (int rr = r + dr, cc = c + dc;
         rr >= 0 && cc >= 0 && rr < rows && cc < cols;
         rr += dr, cc += dc)
    {
        char ch = board[rr][cc];
        if (ch == '#' || ch == '@') {
            // blocked by wall/mine
            return false;
        }
        if (std::isdigit(ch)) {
            int pid = ch - '0';
            if (pid != myPlayerIndex) {
                return true;
            }
            else {
                return false;
            }
        }
    }
    return false;
}

ActionRequest AggressiveTankAI_212934582_323964676::getAction() {
    if (!_gotBattleInfo) {
        return ActionRequest::GetBattleInfo;
    }
    _gotBattleInfo = false;

    // Use member variables that get updated via updateBattleInfo()
    // Match HW2 exactly: _x should be ROW, _y should be COLUMN
    int startR = static_cast<int>(_x);  // row (same as HW2)
    int startC = static_cast<int>(_y);  // column (same as HW2)
    Direction startD = _direction;
    
    // 1) Quick "can we already shoot?" check:
    if (_shellsRemaining > 0 &&
        canShootFrom(_board, _enemyPositions,
                     static_cast<int>(_rows),
                     static_cast<int>(_cols),
                     startR, startC, startD, _playerIndex))
    {
        return ActionRequest::Shoot;
    }

    // 2) BFS over (r,c,d) states to find the shortest path to a shooting state:
    int R = static_cast<int>(_rows);
    int C = static_cast<int>(_cols);
    static constexpr int ND = 8;
    
    
    if (startR < 0 || startR >= R || startC < 0 || startC >= C) {
        return ActionRequest::RotateRight90;
    }

    // visited[r][c][di] = whether (r,c,direction=di) was enqueued
    std::vector<std::vector<std::array<bool,ND>>> visited(
        R, std::vector<std::array<bool,ND>>(C, {false,false,false,false,false,false,false,false})
    );

    struct Parent {
        int pr, pc;
        Direction pd;
        ActionRequest actionTaken;
    };
    // parent[r][c][di] = how we reached (r,c,di)
    std::vector<std::vector<std::array<Parent,ND>>> parent(
        R, std::vector<std::array<Parent,ND>>(C)
    );

    struct Node { int r, c; Direction d; };
    std::queue<Node> q;

    // mark start as visited
    visited[startR][startC][static_cast<int>(startD)] = true;
    parent[startR][startC][static_cast<int>(startD)] = { -1, -1, startD, ActionRequest::DoNothing };
    q.push({ startR, startC, startD });

    bool foundGoal = false;
    Node goalNode{0,0,Direction::UP};

    while (!q.empty() && !foundGoal) {
        auto [cr, cc, cd] = q.front();
        q.pop();

        // Check if from (cr,cc) facing cd we can shoot an enemy:
        if (_shellsRemaining > 0 &&
            canShootFrom(_board, _enemyPositions, R, C,
                         cr, cc, cd, _playerIndex))
        {
            goalNode = Node{ cr, cc, cd };
            foundGoal = true;
            break;
        }

        // Expand neighbors: RotateRight45
        {
            Direction ndir = DirectionUtils::rotate45cw(cd);
            int di = static_cast<int>(ndir);
            if (!visited[cr][cc][di]) {
                visited[cr][cc][di] = true;
                parent[cr][cc][di] = { cr, cc, cd, ActionRequest::RotateRight45 };
                q.push({ cr, cc, ndir });
            }
        }
        // RotateLeft45
        {
            Direction ndir = DirectionUtils::rotate45ccw(cd);
            int di = static_cast<int>(ndir);
            if (!visited[cr][cc][di]) {
                visited[cr][cc][di] = true;
                parent[cr][cc][di] = { cr, cc, cd, ActionRequest::RotateLeft45 };
                q.push({ cr, cc, ndir });
            }
        }
        // RotateRight90
        {
            Direction ndir = DirectionUtils::rotate90(cd, /*cw=*/true);
            int di = static_cast<int>(ndir);
            if (!visited[cr][cc][di]) {
                visited[cr][cc][di] = true;
                parent[cr][cc][di] = { cr, cc, cd, ActionRequest::RotateRight90 };
                q.push({ cr, cc, ndir });
            }
        }
        // RotateLeft90
        {
            Direction ndir = DirectionUtils::rotate90(cd, /*cw=*/false);
            int di = static_cast<int>(ndir);
            if (!visited[cr][cc][di]) {
                visited[cr][cc][di] = true;
                parent[cr][cc][di] = { cr, cc, cd, ActionRequest::RotateLeft90 };
                q.push({ cr, cc, ndir });
            }
        }

        // MoveForward
        {
            auto [dr, dc] = DirectionUtils::toVector(cd);
            int nr = cr + dr;
            int nc = cc + dc;
            if (nr >= 0 && nc >= 0 && nr < R && nc < C) {
                bool blocked = false;
                char ch = _board[nr][nc];
                if (ch == '#' || ch == '@' || std::isdigit(ch)) blocked = true;
                if (!blocked) {
                    // diagonal-corner check:
                    if (nr != cr && nc != cc) {
                        if (_board[cr][nc] == '#' || _board[cr][nc] == '@' || std::isdigit(_board[cr][nc]) ||
                            _board[nr][cc] == '#' || _board[nr][cc] == '@' || std::isdigit(_board[nr][cc]))
                        {
                            blocked = true;
                        }
                    }
                }
                if (!blocked) {
                    int di = static_cast<int>(cd);
                    if (!visited[nr][nc][di]) {
                        visited[nr][nc][di] = true;
                        parent[nr][nc][di] = { cr, cc, cd, ActionRequest::MoveForward };
                        q.push({ nr, nc, cd });
                    }
                }
            }
        }

        // MoveBackward
        {
            Direction backDir = DirectionUtils::rotate180(cd);
            auto [drB, dcB] = DirectionUtils::toVector(backDir);
            int nr = cr + drB;
            int nc = cc + dcB;
            if (nr >= 0 && nc >= 0 && nr < R && nc < C) {
                bool blocked = false;
                char ch = _board[nr][nc];
                if (ch == '#' || ch == '@' || std::isdigit(ch)) blocked = true;
                if (!blocked) {
                    if (nr != cr && nc != cc) {
                        if (_board[cr][nc] == '#' || _board[cr][nc] == '@' || std::isdigit(_board[cr][nc]) ||
                            _board[nr][cc] == '#' || _board[nr][cc] == '@' || std::isdigit(_board[nr][cc]))
                        {
                            blocked = true;
                        }
                    }
                }
                if (!blocked) {
                    int di = static_cast<int>(cd);
                    if (!visited[nr][nc][di]) {
                        visited[nr][nc][di] = true;
                        parent[nr][nc][di] = { cr, cc, cd, ActionRequest::MoveBackward };
                        q.push({ nr, nc, cd });
                    }
                }
            }
        }
    } // end BFS

    // 3) If BFS found a goal state reconstruct the path
    if (foundGoal) {
        std::vector<ActionRequest> reversed;
        Node node = goalNode;
        while (!(node.r == startR && node.c == startC && node.d == startD)) {
            Parent p = parent[node.r][node.c][static_cast<int>(node.d)];
            reversed.push_back(p.actionTaken);
            node = { p.pr, p.pc, p.pd };
        }
        // The very last action in reversed[] is the first move from (startR,startC,startD)
        ActionRequest firstMove = reversed.back();
        return firstMove;
    }

    // 4) Fallback: no reachable shooting state - spin in place
    return ActionRequest::RotateRight90;
}

void AggressiveTankAI_212934582_323964676::findEnemies() {
    _enemyPositions.clear();
    
    // Print the actual board
    for (int r = 0; r < static_cast<int>(_board.size()); ++r) {
        (void)r;
    }
    
    for (int r = 0; r < static_cast<int>(_rows); ++r) {
        for (int c = 0; c < static_cast<int>(_cols); ++c) {
            if (r < static_cast<int>(_board.size()) && c < static_cast<int>(_board[r].length())) {
                char ch = _board[r][c];
                if (std::isdigit(ch)) {
                    int pid = ch - '0';
                    if (pid != _playerIndex) {
                        _enemyPositions.emplace_back(r, c);
                    }
                }
            }
        }
    }
}

bool AggressiveTankAI_212934582_323964676::isCellBlocked(int r, int c) const {
    // Out of bounds - blocked
    if (r < 0 || c < 0 ||
        r >= static_cast<int>(_rows) ||
        c >= static_cast<int>(_cols)) {
        return true;
    }
    char ch = _board[r][c];
    // wall or mine or any tank digit -- blocked
    if (ch == '#' || ch == '@' || std::isdigit(ch)) {
        return true;
    }
    return false;
}

bool AggressiveTankAI_212934582_323964676::hasLineOfSight(int fromR, int fromC, int toR, int toC) const {
    // Simple line of sight check - can be enhanced
    int dr = toR - fromR;
    int dc = toC - fromC;
    
    // Check if path is blocked
    int steps = std::max(std::abs(dr), std::abs(dc));
    if (steps == 0) return true;
    
    for (int i = 1; i < steps; ++i) {
        int r = fromR + (dr * i) / steps;
        int c = fromC + (dc * i) / steps;
        if (isCellBlocked(r, c)) {
            return false;
        }
    }
    return true;
}

Direction AggressiveTankAI_212934582_323964676::directionBetween(int fromR, int fromC, int toR, int toC) const {
    // Calculate direction from one point to another
    int dr = toR - fromR;
    int dc = toC - fromC;
    
    if (dr == 0 && dc == 0) return _direction; // Same position
    
    // Determine primary direction
    if (std::abs(dr) > std::abs(dc)) {
        return (dr > 0) ? Direction::DOWN : Direction::UP;
    } else {
        return (dc > 0) ? Direction::RIGHT : Direction::LEFT;
    }
}

bool AggressiveTankAI_212934582_323964676::isValidDiagonal(int r, int c, int nr, int nc) const {
    // Check if diagonal movement is valid (no corner blocking)
    if (r == nr || c == nc) return true; // Not diagonal
    
    // Check if corners are blocked
    if (isCellBlocked(r, nc) || isCellBlocked(nr, c)) {
        return false;
    }
    
    return true;
}

void AggressiveTankAI_212934582_323964676::computePathToNearestEnemy() {
    // This is a placeholder - the main logic is in getAction() with BFS
    _currentPath.clear();
}

} // namespace Algorithm_212934582_323964676

// Register the aggressive tank algorithm implementation
using AggressiveAI = Algorithm_212934582_323964676::AggressiveTankAI_212934582_323964676;
REGISTER_TANK_ALGORITHM(AggressiveAI)


