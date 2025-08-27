#include "TankAlgorithm_322837089_212141253.h"
#include <queue>
#include <algorithm>
#include <cmath>
#include "../common/TankAlgorithmRegistration.h"
#include "TankAlgorithm_322837089_212141253.h"
#include <iostream> // debug

// Bring the namespace into scope for the macro:
using namespace Algorithm_322837089_212141253;

// Direction vectors for orientations 0-7 (U, UR, R, DR, D, DL, L, UL)
static const int dirDX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int dirDY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

// -----------------------------------------------------------------------------
// ----------------------  TankAlgorithm_322837089_212141253  (Attacker) ------------------------
// -----------------------------------------------------------------------------
TankAlgorithm_322837089_212141253::TankAlgorithm_322837089_212141253(int player_index, int tank_index)
    : playerIndex(player_index), tankIndex(tank_index) {}

void TankAlgorithm_322837089_212141253::setInitialState(int orient) {
    posX = 0; posY = 0; orientation = orient;
    ammo = 0; shootCooldown = 0; backwardWait = 0;
    infoReceived = false; turnsSinceInfo = 0;
}

// Breadth-First Search toward nearest enemy
void TankAlgorithm_322837089_212141253::computePath() {
    currentPath.clear();
    if (staticMap.empty()) return;
    const int h = mapHeight, w = mapWidth;
    Position1 start{posX, posY};

    // find closest enemy cell in snapshot
    Position1 enemy{-1, -1};
    int bestDist = 1e9;

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            char c = staticMap[y][x];
            if ((playerIndex == 1 && c == '2') || (playerIndex == 2 && c == '1')) {
                int d = std::abs(x - posX) + std::abs(y - posY);
                if (d < bestDist) { bestDist = d; enemy = {x, y}; }
            }
        }
    if (enemy.x == -1) return;

    std::queue<Position1> q; q.push(start);
    std::vector<std::vector<char>> seen(h, std::vector<char>(w, 0));
    std::vector<std::vector<Position1>> parent(h, std::vector<Position1>(w, {-1, -1}));
    seen[start.y][start.x] = 1;

    while (!q.empty()) {
        Position1 p = q.front(); q.pop();
        if (p == enemy) break;
        for (int i = 0; i < 8; ++i) {
            Position1 n{(p.x + dirDX[i] + w) % w, (p.y + dirDY[i] + h) % h};
            if (seen[n.y][n.x]) continue;
            char cell = staticMap[n.y][n.x];
            if (isBlocking(cell) && !(n == enemy)) continue; // allow stepping on enemy tile
            seen[n.y][n.x] = 1;
            parent[n.y][n.x] = p;
            q.push(n);
        }
    }
    if (!seen[enemy.y][enemy.x]) return; // no path

    // reconstruct path
    for (Position1 p = enemy; !(p == start); p = parent[p.y][p.x]) currentPath.push_back(p);
    std::reverse(currentPath.begin(), currentPath.end());
}
/*
ActionRequest TankAlgorithm_322837089_212141253::getAction() {
    std::cout << "entered tank algo get action" << "\n";
    const int INFO_PERIOD = 4;                   // refresh every 4 turns
    if (!infoReceived || turnsSinceInfo >= INFO_PERIOD)
        return ActionRequest::GetBattleInfo;
    ++turnsSinceInfo;

    if (shootCooldown > 0) --shootCooldown;
    if (backwardWait > 0) { --backwardWait; return ActionRequest::DoNothing; }

    // opportunistic shoot (ray-cast)
    int dirX = dirDX[orientation], dirY = dirDY[orientation];
    for (int cx = (posX + dirX + mapWidth) % mapWidth,
             cy = (posY + dirY + mapHeight) % mapHeight, steps = 0;
         steps < std::max(mapWidth, mapHeight);
         ++steps, cx = (cx + dirX + mapWidth) % mapWidth,
                  cy = (cy + dirY + mapHeight) % mapHeight) {
        char c = staticMap[cy][cx];
        if (c == '#') break;
        if ((playerIndex == 1 && c == '2') || (playerIndex == 2 && c == '1')) {
            if (ammo > 0 && shootCooldown == 0) {
                --ammo; shootCooldown = 4; turnsSinceInfo = 0; return ActionRequest::Shoot;
            }
            break;
        }
    }
    // Check if enemy is adjacent
    for (int i = 0; i < 8; ++i) {
        int nx = (posX + dirDX[i] + mapWidth) % mapWidth;
        int ny = (posY + dirDY[i] + mapHeight) % mapHeight;
        char c = staticMap[ny][nx];
        if ((playerIndex == 1 && c == '2') || (playerIndex == 2 && c == '1')) {
            if (i == orientation) {
                if (ammo > 0 && shootCooldown == 0) {
                    --ammo; shootCooldown = 4; turnsSinceInfo = 0;
                    return ActionRequest::Shoot;
                }
            } else {
                int cw = (i - orientation + 8) % 8;
                int ccw = (orientation - i + 8) % 8;
                if (cw <= ccw) {
                    orientation = (orientation + (cw >= 2 ? 2 : 1)) % 8;
                    return (cw >= 2 ? ActionRequest::RotateRight90 : ActionRequest::RotateRight45);
                } else {
                    orientation = (orientation + 8 - (ccw >= 2 ? 2 : 1)) % 8;
                    return (ccw >= 2 ? ActionRequest::RotateLeft90 : ActionRequest::RotateLeft45);
                }
            }
        }
    }

    // maintain / recompute path
    if (currentPath.empty()) computePath();
    if (currentPath.empty()) return ActionRequest::DoNothing;

    // path invalid?
    Position1 next = currentPath.front();
    if (isBlocking(staticMap[next.y][next.x])) { currentPath.clear(); turnsSinceInfo = INFO_PERIOD; return ActionRequest::DoNothing; }

    if (posX == next.x && posY == next.y) {
        currentPath.erase(currentPath.begin());
        if (currentPath.empty()) return ActionRequest::DoNothing;
        next = currentPath.front();
    }

    int dx = (next.x - posX + mapWidth) % mapWidth; if (dx >  mapWidth/2) dx -= mapWidth;
    int dy = (next.y - posY + mapHeight) % mapHeight; if (dy >  mapHeight/2) dy -= mapHeight;
    if (dx != 0) dx /= std::abs(dx);
    if (dy != 0) dy /= std::abs(dy);

    int desiredOri = orientation;
    for (int i = 0; i < 8; ++i) if (dirDX[i] == dx && dirDY[i] == dy) { desiredOri = i; break; }

    if (desiredOri == orientation) {
        int fx = (posX + dirDX[orientation] + mapWidth) % mapWidth;
        int fy = (posY + dirDY[orientation] + mapHeight) % mapHeight;
        if (!isBlocking(staticMap[fy][fx])) return ActionRequest::MoveForward;
        turnsSinceInfo = INFO_PERIOD; // obstacle gotten in way – force refresh
        return ActionRequest::RotateRight90;
    }

    int cw = (desiredOri - orientation + 8) % 8;
    int ccw = (orientation - desiredOri + 8) % 8;
    if (cw <= ccw) { orientation = (orientation + (cw >= 2 ? 2 : 1)) % 8; return (cw >= 2 ? ActionRequest::RotateRight90 : ActionRequest::RotateRight45); }
    orientation = (orientation + 8 - (ccw >= 2 ? 2 : 1)) % 8;
    return (ccw >= 2 ? ActionRequest::RotateLeft90 : ActionRequest::RotateLeft45);
}
*/

ActionRequest TankAlgorithm_322837089_212141253::getAction() {

    // ---- Helpers to prevent OOB on staticMap ----
    auto wrap = [](int v, int m) -> int {
        if (m <= 0) return 0;
        int r = v % m;
        return (r < 0) ? r + m : r;
    };
    auto cellAt = [&](int y, int x) -> char {
        if (staticMap.empty()) return '#';
        // y wraps by number of rows we actually have
        const int H = static_cast<int>(staticMap.size());
        const int yy = wrap(y, H);
        const std::string& row = staticMap[yy];
        if (row.empty()) return '#';
        // x wraps by actual row length (rows may be uneven!)
        const int Wrow = static_cast<int>(row.size());
        const int xx = wrap(x, Wrow);
        return row[xx];
    };
    auto safeWrapX = [&](int x) -> int {
        // prefer declared mapWidth, but fall back to row size via cellAt
        return wrap(x, static_cast<int>(mapWidth));
    };
    auto safeWrapY = [&](int y) -> int {
        return wrap(y, static_cast<int>(mapHeight));
    };

    // normalize orientation before using dirDX/dirDY
    orientation = wrap(orientation, 8);

    // guard empty map / zero dims
    if (mapWidth == 0 || mapHeight == 0 || staticMap.empty()) {
        return ActionRequest::DoNothing;
    }

    // ---- info refresh / cooldowns ----
    const int INFO_PERIOD = 4;
    if (!infoReceived || turnsSinceInfo >= INFO_PERIOD)
        return ActionRequest::GetBattleInfo;
    ++turnsSinceInfo;


    if (shootCooldown > 0) --shootCooldown;
    if (backwardWait > 0) { --backwardWait; return ActionRequest::DoNothing; }

    // ---- opportunistic shoot (ray-cast) ----
    int dirX = dirDX[orientation], dirY = dirDY[orientation];
    int cx = safeWrapX(posX + dirX);
    int cy = safeWrapY(posY + dirY);
    const int limit = static_cast<int>(std::max(mapWidth, mapHeight));

    for (int steps = 0; steps < limit; ++steps) {
        char c = cellAt(cy, cx);
        if (c == '#') break;
        if ((playerIndex == 1 && c == '2') || (playerIndex == 2 && c == '1')) {
            if (ammo > 0 && shootCooldown == 0) {
                --ammo; shootCooldown = 4; turnsSinceInfo = 0;
                return ActionRequest::Shoot;
            }
            break;
        }
        cx = safeWrapX(cx + dirX);
        cy = safeWrapY(cy + dirY);
    }

    // ---- adjacent enemy check (no raw staticMap indexing) ----
    for (int i = 0; i < 8; ++i) {
        int nx = safeWrapX(posX + dirDX[i]);
        int ny = safeWrapY(posY + dirDY[i]);
        char c = cellAt(ny, nx);
        if ((playerIndex == 1 && c == '2') || (playerIndex == 2 && c == '1')) {
            if (i == orientation) {
                if (ammo > 0 && shootCooldown == 0) {
                    --ammo; shootCooldown = 4; turnsSinceInfo = 0;
                    return ActionRequest::Shoot;
                }
            } else {
                int cw = (i - orientation + 8) % 8;
                int ccw = (orientation - i + 8) % 8;
                if (cw <= ccw) {
                    orientation = (orientation + (cw >= 2 ? 2 : 1)) % 8;
                    return (cw >= 2 ? ActionRequest::RotateRight90 : ActionRequest::RotateRight45);
                } else {
                    orientation = (orientation + 8 - (ccw >= 2 ? 2 : 1)) % 8;
                    return (ccw >= 2 ? ActionRequest::RotateLeft90 : ActionRequest::RotateLeft45);
                }
            }
        }
    }

    // ---- maintain / recompute path ----
    if (currentPath.empty()) computePath();
    if (currentPath.empty()) return ActionRequest::DoNothing;

    // ---- validate next step safely ----
    Position1 next = currentPath.front();
    // normalize next to board to avoid OOB from computePath()
    next.x = safeWrapX(next.x);
    next.y = safeWrapY(next.y);

    if (isBlocking(cellAt(next.y, next.x))) {
        currentPath.clear();
        turnsSinceInfo = INFO_PERIOD; // force refresh
        return ActionRequest::DoNothing;
    }

    if (posX == next.x && posY == next.y) {
        currentPath.erase(currentPath.begin());
        if (currentPath.empty()) return ActionRequest::DoNothing;
        next = currentPath.front();
        next.x = safeWrapX(next.x);
        next.y = safeWrapY(next.y);
    }

    int dx = (next.x - posX + static_cast<int>(mapWidth)) % static_cast<int>(mapWidth);
    if (dx > static_cast<int>(mapWidth) / 2) dx -= static_cast<int>(mapWidth);
    int dy = (next.y - posY + static_cast<int>(mapHeight)) % static_cast<int>(mapHeight);
    if (dy > static_cast<int>(mapHeight) / 2) dy -= static_cast<int>(mapHeight);
    if (dx != 0) dx /= std::abs(dx);
    if (dy != 0) dy /= std::abs(dy);

    int desiredOri = orientation;
    for (int i = 0; i < 8; ++i) {
        if (dirDX[i] == dx && dirDY[i] == dy) { desiredOri = i; break; }
    }

    if (desiredOri == orientation) {
        int fx = safeWrapX(posX + dirDX[orientation]);
        int fy = safeWrapY(posY + dirDY[orientation]);
        if (!isBlocking(cellAt(fy, fx))) return ActionRequest::MoveForward;
        turnsSinceInfo = INFO_PERIOD; // obstacle appeared – refresh
        return ActionRequest::RotateRight90;
    }

    int cw = (desiredOri - orientation + 8) % 8;
    int ccw = (orientation - desiredOri + 8) % 8;
    if (cw <= ccw) {
        orientation = (orientation + (cw >= 2 ? 2 : 1)) % 8;
        return (cw >= 2 ? ActionRequest::RotateRight90 : ActionRequest::RotateRight45);
    }
    orientation = (orientation + 8 - (ccw >= 2 ? 2 : 1)) % 8;
    return (ccw >= 2 ? ActionRequest::RotateLeft90 : ActionRequest::RotateLeft45);
}

void TankAlgorithm_322837089_212141253::updateBattleInfo(BattleInfo& info) {
    auto& bi = static_cast<MyBattleInfo&>(info);
    if (bi.self_x == (size_t)-1 || bi.self_y == (size_t)-1) {
        // no valid self position in snapshot; ask again next turn
        std::cout << "no valid self position in snapshot; ask again next turn" << "\n";
        infoReceived = false;
        return;
    }
    ammo = bi.initial_shells;
    staticMap = bi.view;
    mapHeight = static_cast<int>(bi.view.size());
    mapWidth  = mapHeight ? static_cast<int>(bi.view[0].size()) : 0;
    posX = static_cast<int>(bi.self_x);
    posY = static_cast<int>(bi.self_y);
    if (posY < mapHeight && posX < mapWidth) staticMap[posY][posX] = ' ';
    infoReceived = true; turnsSinceInfo = 0; currentPath.clear();
}

// -----------------------------------------------------------------------------
// A3 factory helper (returns the alias std::function<...>)
// -----------------------------------------------------------------------------
TankAlgorithmFactory make_my_tank_algorithm_factory() {
    return [](int player_index, int tank_index) -> std::unique_ptr<TankAlgorithm> {
        std::unique_ptr<TankAlgorithm> alg;
        //if (player_index == 1) {
            alg = std::make_unique<TankAlgorithm_322837089_212141253>(player_index, tank_index);
        //} else {
        //    alg = std::make_unique<TankAlgorithm_322837089_2121412532>(player_index, tank_index);
        //}
        const int orient = (player_index == 1 ? 6 : 2); // L or R
        //if (player_index == 1) {
            static_cast<TankAlgorithm_322837089_212141253*>(alg.get())->setInitialState(orient);
        //} else {
        //    static_cast<TankAlgorithm_322837089_2121412532*>(alg.get())->setInitialState(orient);
        //}
        return alg;
    };
}

// MUST be at global scope (not inside any namespace or function):
REGISTER_TANK_ALGORITHM(TankAlgorithm_322837089_212141253);
