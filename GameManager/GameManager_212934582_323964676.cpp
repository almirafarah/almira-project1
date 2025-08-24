#include "GameManager_212934582_323964676.h"
#include "MySatelliteView.h"
#include "../UserCommon/MyBattleInfo_212934582_323964676.h"
#include "../common/ActionRequest.h"
#include "../UserCommon/Utils_212934582_323964676.h"  // For Direction and DirectionUtils
#include <iostream>
#include <algorithm>

namespace GameManager_212934582_323964676 {

// Import utilities from UserCommon
using UserCommon_212934582_323964676::Direction;
using UserCommon_212934582_323964676::DirectionUtils;
using UserCommon_212934582_323964676::MyBattleInfo;

GameManager_212934582_323964676::GameManager_212934582_323964676(bool verbose)
    : verbose_(verbose), last_hit_position_(std::make_pair(0, 0)), last_hit_player_(0) {
}

GameResult GameManager_212934582_323964676::run(
    size_t map_width,
    size_t map_height,
    const SatelliteView& map,  // Map provided by Simulator - GameManager does NOT read files
    string map_name,
    size_t max_steps,
    size_t num_shells,
    Player& player1, string name1,  // Player references - ownership stays with Simulator
    Player& player2, string name2,  // Player references - ownership stays with Simulator
    TankAlgorithmFactory player1_tank_algo_factory,
    TankAlgorithmFactory player2_tank_algo_factory) {
    (void)map_name;
    (void)player1;
    (void)name1;
    (void)player2;
    (void)name2;
    if (verbose_) {
        std::cout << "GameManager::run() called with " << map_width << "x" << map_height << std::endl;
    }

    // Initialize game board from SatelliteView (extract once, store locally like HW2)
    board_.clear();
    board_.resize(map_height);
    for (size_t y = 0; y < map_height; ++y) {
        board_[y].resize(map_width, ' ');  // Initialize with spaces
        for (size_t x = 0; x < map_width; ++x) {
            try {
                board_[y][x] = map.getObjectAt(x, y);
            } catch (...) {
                board_[y][x] = '?';  // Safe fallback
            }
        }
    }



    // Find initial tank positions and create tank states
    std::vector<TankState> player1_tanks = findTanks(map, map_width, map_height, 1, num_shells);
    std::vector<TankState> player2_tanks = findTanks(map, map_width, map_height, 2, num_shells);

    if (verbose_) {
        std::cout << "Player 1 has " << player1_tanks.size() << " tanks" << std::endl;
        std::cout << "Player 2 has " << player2_tanks.size() << " tanks" << std::endl;
    }

    // Basic game loop - run through steps (limited to 3 steps for testing)
    size_t current_step = 0;
    bool game_over = false;
    size_t debug_max_steps = max_steps;  // Use full max_steps for combat testing

    while (current_step < debug_max_steps && !game_over) {
        current_step++;

        if (verbose_) {
            std::cout << "\n=== Step " << current_step << " of " << debug_max_steps << " ===" << std::endl;
        }

        // ADVANCE ALL FLYING SHELLS (from HW2 flying shell system)
        if (verbose_) {
            std::cout << "  Advancing " << live_shells_.size() << " flying shells..." << std::endl;
        }

        std::vector<ShellState> still_flying;
        for (auto& shell : live_shells_) {
            // toVector returns (dRow, dCol)
            auto [dRow, dCol] = DirectionUtils::toVector(static_cast<Direction>(shell.dir));
            int new_x = static_cast<int>(shell.x) + dCol; // column += col delta
            int new_y = static_cast<int>(shell.y) + dRow; // row    += row delta

            // Check if shell goes out of bounds
            if (new_x < 0 || new_y < 0 ||
                static_cast<size_t>(new_x) >= map_width ||
                static_cast<size_t>(new_y) >= map_height) {
                if (verbose_) {
                    std::cout << "    Shell at (" << shell.x << "," << shell.y << ") goes out of bounds" << std::endl;
                }
                continue;  // Shell destroyed
            }

            // Check if shell hits a wall
            if (board_[new_y][new_x] == '#') {
                if (verbose_) {
                    std::cout << "    Shell at (" << shell.x << "," << shell.y << ") hits wall at (" << new_x << "," << new_y << ")" << std::endl;
                }
                continue;  // Shell destroyed
            }

            // Check if shell hits a tank
            char cell = board_[new_y][new_x];
            if (cell == '1' || cell == '2') {
                if (verbose_) {
                    std::cout << "    Shell at (" << shell.x << "," << shell.y << ") hits tank '" << cell << "' at (" << new_x << "," << new_y << ")!" << std::endl;
                }

                // Clear the tank from board
                board_[new_y][new_x] = ' ';

                // Store hit information for tank destruction
                last_hit_position_ = std::make_pair(new_x, new_y);
                last_hit_player_ = (cell == '1') ? 1 : 2;

                continue;  // Shell destroyed
            }

            // Shell continues flying
            shell.x = static_cast<size_t>(new_x);
            shell.y = static_cast<size_t>(new_y);
            still_flying.push_back(shell);

            if (verbose_) {
                std::cout << "    Shell advances to (" << shell.x << "," << shell.y << ")" << std::endl;
            }
        }

        // Update live shells list
        live_shells_.swap(still_flying);

        if (verbose_) {
            std::cout << "  " << live_shells_.size() << " shells still flying" << std::endl;
        }

        // Execute player 1 turns
        for (auto& tank : player1_tanks) {
            if (!tank.is_alive) continue;  // Skip dead tanks

            if (verbose_) {
                std::cout << "Player 1 Tank " << tank.tank_id << " at (" << tank.x << "," << tank.y
                          << ") facing " << static_cast<int>(tank.facing)
                          << " shells:" << tank.shells_remaining << std::endl;
            }

            // Create tank algorithm instance
            auto tank_ai = player1_tank_algo_factory(tank.player_id, tank.tank_id);

            // Create battle info for this tank (HW2 integration!)
            MyBattleInfo battle_info = createBattleInfoForTank(tank, map_width, map_height);

            // Provide the tank AI with battle information (HW3 pattern: only BattleInfo)
            tank_ai->updateBattleInfo(battle_info);

            // Get action from AI
            ActionRequest action = tank_ai->getAction();

            if (verbose_) {
                std::cout << "  Action: " << actionRequestToString(action) << std::endl;
            }

            // Execute the action!
            bool success = executeAction(tank, action, map_width, map_height);

            // Check if we need to mark any tank as dead from shell advancement
            if (last_hit_player_ != 0) {
                // Find and mark the hit tank as dead
                if (last_hit_player_ == 1) {
                    for (auto& t : player1_tanks) {
                        if (t.x == last_hit_position_.first && t.y == last_hit_position_.second) {
                            t.is_alive = false;
                            if (verbose_) {
                                std::cout << "    *** Player 1 Tank destroyed at (" << t.x << "," << t.y << ") ***" << std::endl;
                            }
                            break;
                        }
                    }
                } else if (last_hit_player_ == 2) {
                    for (auto& t : player2_tanks) {
                        if (t.x == last_hit_position_.first && t.y == last_hit_position_.second) {
                            t.is_alive = false;
                            if (verbose_) {
                                std::cout << "    *** Player 2 Tank destroyed at (" << t.x << "," << t.y << ") ***" << std::endl;
                            }
                            break;
                        }
                    }
                }
                // Reset hit tracking
                last_hit_player_ = 0;
            }

            if (verbose_ && !success) {
                std::cout << "    Action failed!" << std::endl;
            }
        }

        // Execute player 2 turns
        for (auto& tank : player2_tanks) {
            if (!tank.is_alive) continue;  // Skip dead tanks

            if (verbose_) {
                std::cout << "Player 2 Tank " << tank.tank_id << " at (" << tank.x << "," << tank.y
                          << ") facing " << static_cast<int>(tank.facing)
                          << " shells:" << tank.shells_remaining << std::endl;
            }

            try {
                // Create tank algorithm instance
                auto tank_ai = player2_tank_algo_factory(tank.player_id, tank.tank_id);

                if (verbose_) {
                    std::cout << "    Tank AI created successfully" << std::endl;
                }

                // Create battle info for this tank (HW2 integration!)
                MyBattleInfo battle_info = createBattleInfoForTank(tank, map_width, map_height);

                if (verbose_) {
                    std::cout << "    Battle info created successfully" << std::endl;
                }

                // Provide the tank AI with battle information (HW3 pattern: only BattleInfo)
                tank_ai->updateBattleInfo(battle_info);

                if (verbose_) {
                    std::cout << "    Battle info updated successfully" << std::endl;
                }

                // Get action from AI
                ActionRequest action = tank_ai->getAction();

                if (verbose_) {
                    std::cout << "    Action: " << actionRequestToString(action) << std::endl;
                }

                // Execute the action!
                bool success = executeAction(tank, action, map_width, map_height);

                // Check if we need to mark any tank as dead from shell advancement
            if (last_hit_player_ != 0) {
                // Find and mark the hit tank as dead
                if (last_hit_player_ == 1) {
                    for (auto& t : player1_tanks) {
                        if (t.x == last_hit_position_.first && t.y == last_hit_position_.second) {
                            t.is_alive = false;
                            if (verbose_) {
                                std::cout << "    *** Player 1 Tank destroyed at (" << t.x << "," << t.y << ") ***" << std::endl;
                            }
                            break;
                        }
                    }
                } else if (last_hit_player_ == 2) {
                    for (auto& t : player2_tanks) {
                        if (t.x == last_hit_position_.first && t.y == last_hit_position_.second) {
                            t.is_alive = false;
                            if (verbose_) {
                                std::cout << "    *** Player 2 Tank destroyed at (" << t.x << "," << t.y << ") ***" << std::endl;
                            }
                            break;
                        }
                    }
                }
                // Reset hit tracking
                last_hit_player_ = 0;
            }

            if (verbose_ && !success) {
                std::cout << "    Action failed!" << std::endl;
            }

            } catch (const std::exception& e) {
                if (verbose_) {
                    std::cout << "    ERROR during Player 2 turn: " << e.what() << std::endl;
                }
                continue;  // Skip this tank's turn if there's an error
            }
        }

        // Check for early game end conditions
        size_t alive_p1_tanks = std::count_if(player1_tanks.begin(), player1_tanks.end(),
                                              [](const TankState& tank) { return tank.is_alive; });
        size_t alive_p2_tanks = std::count_if(player2_tanks.begin(), player2_tanks.end(),
                                              [](const TankState& tank) { return tank.is_alive; });

        if (verbose_) {
            std::cout << "Tanks remaining: Player 1=" << alive_p1_tanks << ", Player 2=" << alive_p2_tanks << std::endl;
        }

        if (alive_p1_tanks == 0 && alive_p2_tanks == 0) {
            game_over = true;
            if (verbose_) {
                std::cout << "Game over: All tanks destroyed (Tie)" << std::endl;
            }
        } else if (alive_p1_tanks == 0) {
            game_over = true;
            if (verbose_) {
                std::cout << "Game over: Player 2 wins - all Player 1 tanks destroyed!" << std::endl;
            }
        } else if (alive_p2_tanks == 0) {
            game_over = true;
            if (verbose_) {
                std::cout << "Game over: Player 1 wins - all Player 2 tanks destroyed!" << std::endl;
            }
        }
    }

    // Create result based on how the game ended
    GameResult result;

    // Count alive tanks at the end
    size_t final_p1_tanks = std::count_if(player1_tanks.begin(), player1_tanks.end(),
                                          [](const TankState& tank) { return tank.is_alive; });
    size_t final_p2_tanks = std::count_if(player2_tanks.begin(), player2_tanks.end(),
                                          [](const TankState& tank) { return tank.is_alive; });

    // Determine winner based on alive tank counts
    if (final_p1_tanks > final_p2_tanks) {
        result.winner = 1;
    } else if (final_p2_tanks > final_p1_tanks) {
        result.winner = 2;
    } else {
        result.winner = 0;  // Tie
    }

    // Determine end reason
    if (final_p1_tanks == 0 && final_p2_tanks == 0) {
        result.reason = GameResult::ALL_TANKS_DEAD;
    } else if (final_p1_tanks == 0 || final_p2_tanks == 0) {
        // One side eliminated before reaching max steps
        result.reason = GameResult::ALL_TANKS_DEAD;
    } else if (current_step >= debug_max_steps) {
        result.reason = GameResult::MAX_STEPS;
    } else {
        result.reason = GameResult::MAX_STEPS;  // Fallback
    }

    result.remaining_tanks = {final_p1_tanks, final_p2_tanks};

    // Add the missing fields required by the assignment
    result.rounds = current_step;

            // Create a final game state snapshot (no highlighting for output)
        result.gameState = std::make_unique<MySatelliteView>(board_, live_shells_);

    if (verbose_) {
        std::cout << "\nGame completed after " << current_step << " steps" << std::endl;
        if (result.winner == 0) {
            std::cout << "Winner: Tie" << std::endl;
        } else {
            std::cout << "Winner: Player " << result.winner << std::endl;
        }
        std::cout << "Reason: " << (result.reason == GameResult::MAX_STEPS ? "Max steps reached" : "All tanks dead") << std::endl;
    }

    return result;
}

std::vector<TankState> GameManager_212934582_323964676::findTanks(
    const SatelliteView& map, size_t width, size_t height, int player_id, size_t shells_per_tank) {

    std::vector<TankState> tanks;
    char player_char = (player_id == 1) ? '1' : '2';  // Convert player_id to character
    int tank_counter = 0;  // Track tank index for each player

    // Scan the entire map looking for the player character ('1' or '2')
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            char cell = map.getObjectAt(x, y);
            if (cell == player_char) {
                tanks.emplace_back(x, y, player_id, tank_counter++, shells_per_tank);

                        // Set strategic initial facing directions for better combat (HW2 standard)
        if (player_id == 1) {
            tanks.back().facing = 6;  // Player 1 faces left (Direction::LEFT = 6)
        } else {
            tanks.back().facing = 2;  // Player 2 faces right (Direction::RIGHT = 2)
        }

                if (verbose_) {
                    std::cout << "Found tank for player " << player_id
                              << " (tank " << tank_counter - 1 << ") at position (" << x << ", " << y
                              << ") facing " << static_cast<int>(tanks.back().facing)
                              << " with " << shells_per_tank << " shells" << std::endl;
                }
            }
        }
    }

    return tanks;
}

std::string GameManager_212934582_323964676::actionRequestToString(ActionRequest req) {
    switch (req) {
        case ActionRequest::MoveForward:   return "MoveForward";
        case ActionRequest::MoveBackward:  return "MoveBackward";
        case ActionRequest::RotateLeft90:  return "RotateLeft90";
        case ActionRequest::RotateRight90: return "RotateRight90";
        case ActionRequest::RotateLeft45:  return "RotateLeft45";
        case ActionRequest::RotateRight45: return "RotateRight45";
        case ActionRequest::Shoot:         return "Shoot";
        case ActionRequest::GetBattleInfo: return "GetBattleInfo";
        case ActionRequest::DoNothing:     return "DoNothing";
    }
    return "Unknown";
}

MySatelliteView GameManager_212934582_323964676::createSatelliteViewForTank(const TankState& tank) const {
    // Create a MySatelliteView that shows the tank's position as '%'
    // and includes all flying shells as '*'
    return MySatelliteView(board_, tank.x, tank.y, live_shells_);
}

MyBattleInfo GameManager_212934582_323964676::createBattleInfoForTank(const TankState& tank, size_t width, size_t height) const {
    // Create a MyBattleInfo with all the information this tank needs
    // Make sure dimensions are reasonable to avoid huge memory allocations
    if (width > 1000 || height > 1000) {
        if (verbose_) {
            std::cout << "Warning: Very large map size " << width << "x" << height << std::endl;
        }
    }



    // TankState uses x=column, y=row. MyBattleInfo expects x=row, y=column.
    return MyBattleInfo(
        height,                    // rows (map height)
        width,                     // cols (map width)
        board_,                    // board (copy of the game board)
        tank.y,                    // x = row
        tank.x,                    // y = column
        tank.facing,               // direction (tank's current facing)
        tank.shells_remaining      // shells_remaining
    );
}

bool GameManager_212934582_323964676::executeAction(TankState& tank, ActionRequest action, size_t map_width, size_t map_height) {
    if (!tank.is_alive) {
        return false;  // Dead tanks can't act
    }

    switch (action) {
        case ActionRequest::DoNothing:
            // Tank does nothing - always succeeds
            if (verbose_) {
                std::cout << "    Tank does nothing" << std::endl;
            }
            return true;

        case ActionRequest::GetBattleInfo:
            // Tank requests battle info - always succeeds (info already provided)
            if (verbose_) {
                std::cout << "    Tank requests battle info" << std::endl;
            }
            return true;

        case ActionRequest::RotateLeft45:
            // Rotate tank 45 degrees counter-clockwise
            tank.facing = static_cast<int>(DirectionUtils::rotate45ccw(static_cast<Direction>(tank.facing)));
            if (verbose_) {
                std::cout << "    Tank rotated left to facing " << static_cast<int>(tank.facing) << std::endl;
            }
            return true;

        case ActionRequest::RotateRight45:
            // Rotate tank 45 degrees clockwise
            tank.facing = static_cast<int>(DirectionUtils::rotate45cw(static_cast<Direction>(tank.facing)));
            if (verbose_) {
                std::cout << "    Tank rotated right to facing " << static_cast<int>(tank.facing) << std::endl;
            }
            return true;

        case ActionRequest::RotateLeft90:
            // Rotate tank 90 degrees counter-clockwise
            tank.facing = static_cast<int>(DirectionUtils::rotate90(static_cast<Direction>(tank.facing), false));  // false = counter-clockwise
            if (verbose_) {
                std::cout << "    Tank rotated left 90° to facing " << static_cast<int>(tank.facing) << std::endl;
            }
            return true;

        case ActionRequest::RotateRight90:
            // Rotate tank 90 degrees clockwise
            tank.facing = static_cast<int>(DirectionUtils::rotate90(static_cast<Direction>(tank.facing), true));   // true = clockwise
            if (verbose_) {
                std::cout << "    Tank rotated right 90° to facing " << static_cast<int>(tank.facing) << std::endl;
            }
            return true;

        case ActionRequest::MoveForward:
        case ActionRequest::MoveBackward:
            return executeMovement(tank, action, map_width, map_height);

        case ActionRequest::Shoot:
            return executeShoot(tank, map_width, map_height);

        default:
            if (verbose_) {
                std::cout << "    Unknown action: " << static_cast<int>(action) << std::endl;
            }
            return false;
    }
}

bool GameManager_212934582_323964676::executeMovement(TankState& tank, ActionRequest action, size_t map_width, size_t map_height) {
    if (!tank.is_alive) {
        return false;
    }

    // Determine movement direction
    int move_direction = tank.facing;
    if (action == ActionRequest::MoveBackward) {
        // For backward movement, face the opposite direction
        move_direction = static_cast<int>(DirectionUtils::rotate180(static_cast<Direction>(tank.facing)));
    }

    // Get movement vector as (dRow, dCol). toVector returns (rowDelta, colDelta)
    auto [dRow, dCol] = DirectionUtils::toVector(static_cast<Direction>(move_direction));

    // Calculate new position
    // TankState stores x=column, y=row
    int new_x = static_cast<int>(tank.x) + dCol;  // column += colDelta
    int new_y = static_cast<int>(tank.y) + dRow;  // row    += rowDelta

    // Check boundaries
    if (new_x < 0 || new_y < 0 ||
        static_cast<size_t>(new_x) >= map_width ||
        static_cast<size_t>(new_y) >= map_height) {
        if (verbose_) {
            std::cout << "    Movement blocked: would go out of bounds ("
                      << new_x << "," << new_y << ")" << std::endl;
        }
        return false;
    }

    // Check collision with walls and obstacles
    // Safety check: ensure the row exists and has data
    if (static_cast<size_t>(new_y) >= board_.size() ||
           board_[static_cast<size_t>(new_y)].empty() ||
           static_cast<size_t>(new_x) >= board_[static_cast<size_t>(new_y)].length()) {
        if (verbose_) {
            std::cout << "    Movement blocked: invalid board position (" << new_x << "," << new_y << ")" << std::endl;
        }
        return false;
    }

    char target_cell = board_[new_y][new_x];  // board_[row][col] = board_[y][x]
    if (target_cell == '#' || target_cell == '@') {
        if (verbose_) {
            std::cout << "    Movement blocked: obstacle '" << target_cell
                      << "' at (" << new_x << "," << new_y << ")" << std::endl;
        }
        return false;
    }

    // Check collision with other tanks (basic version - tank characters '1' or '2')
    if (target_cell == '1' || target_cell == '2') {
        if (verbose_) {
            std::cout << "    Movement blocked: tank '" << target_cell
                      << "' at (" << new_x << "," << new_y << ")" << std::endl;
        }
        return false;
    }

    // Movement is valid - update tank position and board
    if (verbose_) {
        std::cout << "    Tank moved from (" << tank.x << "," << tank.y
                  << ") to (" << new_x << "," << new_y << ")" << std::endl;
    }

    // Update board: clear old position, set new position
    char tank_char = (tank.player_id == 1) ? '1' : '2';

    // Safety check: ensure old position is valid
    if (tank.y < board_.size() && !board_[tank.y].empty() && tank.x < board_[tank.y].length()) {
        board_[tank.y][tank.x] = ' ';  // Clear old position
    }

    tank.x = static_cast<size_t>(new_x);
    tank.y = static_cast<size_t>(new_y);

    // Safety check: ensure new position is valid
    if (tank.y < board_.size() && !board_[tank.y].empty() && tank.x < board_[tank.y].length()) {
        board_[tank.y][tank.x] = tank_char;  // Set new position
    }

    return true;
}

// Helper function to check if tank can shoot an enemy from current position and direction (HW2 logic)
bool GameManager_212934582_323964676::canShootFrom(size_t x, size_t y, int facing, int player_id) const {
    // toVector returns (dRow, dCol). Our board indexing is board_[row][col] => [y][x].
    auto [dRow, dCol] = DirectionUtils::toVector(static_cast<Direction>(facing));

    // Ray-cast in the direction the tank is facing
    for (int rx = static_cast<int>(x) + dCol, ry = static_cast<int>(y) + dRow;
         rx >= 0 && ry >= 0 && static_cast<size_t>(rx) < board_[0].size() && static_cast<size_t>(ry) < board_.size();
         rx += dCol, ry += dRow)
    {
        // Safety check: ensure the row exists and has data
        if (static_cast<size_t>(ry) >= board_.size() || board_[ry].empty() || static_cast<size_t>(rx) >= board_[ry].length()) {
            break;  // Out of bounds or empty row
        }

        char ch = board_[ry][rx];
        if (ch == '#' || ch == '@') {
            // blocked by wall/mine
            return false;
        }
        if (ch == '1' || ch == '2') {
            int target_player = (ch == '1') ? 1 : 2;
            if (target_player != player_id) {
                // Found an enemy tank - can shoot!
                return true;
            } else {
                // Found our own tank - can't shoot through it
                return false;
            }
        }
    }
    return false;  // No enemy found in this direction
}

bool GameManager_212934582_323964676::executeShoot(TankState& tank, size_t map_width, size_t map_height) {
    if (!tank.is_alive) {
        return false;
    }

    // Check if tank has shells remaining
    if (tank.shells_remaining == 0) {
        if (verbose_) {
            std::cout << "    Shooting failed: no shells remaining" << std::endl;
        }
        return false;
    }

    // Check if tank can actually hit an enemy from current position and direction (HW2 validation)
    if (!canShootFrom(tank.x, tank.y, tank.facing, tank.player_id)) {
        if (verbose_) {
            std::cout << "    Shooting failed: no enemy in line of sight from position ("
                      << tank.x << "," << tank.y << ") facing direction " << static_cast<int>(tank.facing) << std::endl;
        }
        return false;  // Don't waste shells if we can't hit anything
    }

    // Consume one shell
    tank.shells_remaining--;

    if (verbose_) {
        std::cout << "    Tank shoots in direction " << static_cast<int>(tank.facing)
                  << " (shells remaining: " << tank.shells_remaining << ")" << std::endl;
    }

    // HW2 behavior: Immediate ray-casting to kill tanks instantly
    // toVector returns (dRow, dCol)
    auto [dRow, dCol] = DirectionUtils::toVector(static_cast<Direction>(tank.facing));

    // Ray-cast until wall or out-of-bounds (HW2 logic)
    for (int rx = static_cast<int>(tank.x) + dCol, ry = static_cast<int>(tank.y) + dRow;
         rx >= 0 && ry >= 0 && static_cast<size_t>(rx) < map_width && static_cast<size_t>(ry) < map_height;
         rx += dCol, ry += dRow)
    {
        char cell = board_[ry][rx];
        if (cell == '#') {
            // Hit wall, stop ray-casting
            break;
        }

        // Check if we hit a tank
        if (cell == '1' || cell == '2') {
            int hit_player = (cell == '1') ? 1 : 2;
            if (hit_player != tank.player_id) {
                // Kill the enemy tank immediately (HW2 behavior)
                if (verbose_) {
                    std::cout << "      Shell immediately hits tank '" << cell
                              << "' at (" << rx << "," << ry << ")!" << std::endl;
                }
                board_[ry][rx] = ' ';
                last_hit_position_ = std::make_pair(rx, ry);
                last_hit_player_ = hit_player;
                return true;  // Tank killed, shell consumed
            } else {
                // Hit our own tank, stop ray-casting
                break;
            }
        }
    }

    // HW2 behavior: No flying shells created - shells are just consumed
    if (verbose_) {
        std::cout << "      Shell fired but no target hit" << std::endl;
    }

    return true;  // Shooting action succeeded (consumed shell)
}

} // namespace GameManager_212934582_323964676

// Auto-registration
#include "../common/GameManagerRegistration.h"
using GameManagerClassAlias = GameManager_212934582_323964676::GameManager_212934582_323964676;
#define GameManager_212934582_323964676 GameManagerClassAlias
REGISTER_GAME_MANAGER(GameManager_212934582_323964676)
#undef GameManager_212934582_323964676