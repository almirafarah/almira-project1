#ifndef GAMEMANAGER_212934582_323964676_H
#define GAMEMANAGER_212934582_323964676_H

#include "../common/AbstractGameManager.h"
#include "../common/GameResult.h"
#include "../common/Player.h"
#include "../common/TankAlgorithm.h"
#include "../common/SatelliteView.h"
#include "../UserCommon/MyBattleInfo_212934582_323964676.h"  // Include the full MyBattleInfo definition
#include <memory>
#include <vector>
#include <array>

namespace GameManager_212934582_323964676 {

// Import UserCommon types
using UserCommon_212934582_323964676::MyBattleInfo;

// Forward declarations
class MySatelliteView;

// Forward declare the Direction enum from Algorithm/Utils.h
// We'll use the fully qualified name to avoid namespace pollution

// Tank state struct to track each tank's properties (enhanced from HW2)
struct TankState {
    int player_id;                  // Which player owns this tank (1 or 2)
    int tank_id;                    // Tank index within the player
    size_t x, y;                    // Position on the map
    int facing;                     // Which direction the tank is facing (0-7, will be cast to Direction when needed)
    size_t shells_remaining;        // How many shells this tank has left
    bool is_alive;                  // Whether the tank is still active
    
    TankState(size_t x, size_t y, int player_id, int tank_id, size_t shells) 
        : player_id(player_id), tank_id(tank_id), x(x), y(y), 
          facing(0), shells_remaining(shells), is_alive(true) {}  // 0 = UP
};

// Shell state struct for tracking flying shells (from HW2)
struct ShellState {
    size_t x, y;                    // Current coordinates of shell
    int dir;                        // Direction in which it's traveling (0-7, will be cast to Direction when needed)
    
    ShellState(size_t x, size_t y, int dir) : x(x), y(y), dir(dir) {}
};

class GameManager_212934582_323964676 : public AbstractGameManager {
public:
    GameManager_212934582_323964676(bool verbose);
    virtual ~GameManager_212934582_323964676() = default;

    // AbstractGameManager interface
    // Note: Simulator provides map as SatelliteView and owns Player objects
    // GameManager receives references and does NOT read files or own Players
    virtual GameResult run(
        size_t map_width,
        size_t map_height,
        const SatelliteView& map,  // Map provided by Simulator
        string map_name,
        size_t max_steps,
        size_t num_shells,
        Player& player1, string name1,  // Reference - ownership with Simulator
        Player& player2, string name2,  // Reference - ownership with Simulator
        TankAlgorithmFactory player1_tank_algo_factory,
        TankAlgorithmFactory player2_tank_algo_factory) override;

private:
    bool verbose_;
    
    // Game state (adapted from HW2)
    std::vector<std::string> board_;        // The static map as strings
    std::vector<ShellState> live_shells_;   // All shells currently in flight
    
    // Tank destruction tracking
    std::pair<size_t, size_t> last_hit_position_;
    int last_hit_player_;
    
    // Helper functions (adapted from HW2)
    std::vector<TankState> findTanks(const SatelliteView& map, size_t width, size_t height, int player_id, size_t shells_per_tank);
    std::string actionRequestToString(ActionRequest req);
    
    // Create a MySatelliteView for a specific tank
    MySatelliteView createSatelliteViewForTank(const TankState& tank) const;
    
    // Create a MyBattleInfo for a specific tank
    MyBattleInfo createBattleInfoForTank(const TankState& tank, size_t width, size_t height) const;
    
    // Execute a tank action (adapted from HW2)
    bool executeAction(TankState& tank, ActionRequest action, size_t map_width, size_t map_height);
    
    // Execute movement with collision detection
    bool executeMovement(TankState& tank, ActionRequest action, size_t map_width, size_t map_height);
    
    // Execute shooting with ray-casting (adapted from HW2)
    bool executeShoot(TankState& tank, size_t map_width, size_t map_height);
    
    // Helper function to check if tank can shoot an enemy (HW2 ray-casting logic)
    bool canShootFrom(size_t x, size_t y, int facing, int player_id) const;
    
    // Helper to find and kill tank at position
    bool killTankAt(size_t x, size_t y, std::vector<TankState>& player1_tanks, std::vector<TankState>& player2_tanks);
};

} // namespace GameManager_212934582_323964676

#endif // GAMEMANAGER_212934582_323964676_H