#ifndef Player_322837089_212141253_H
#define Player_322837089_212141253_H

#include "../common/BattleInfo.h"
#include "../common/Player.h"
#include "../common/TankAlgorithm.h"   // for TankAlgorithm in the method signature
#include "../common/SatelliteView.h"   // for SatelliteView in the method signature
#include "../common/PlayerRegistration.h"
#include <memory>
#include <vector>
#include <string>

namespace Algorithm_322837089_212141253{

// -------------------------------------
// Local Battle info class (no namespace)
// -------------------------------------
class MyBattleInfo : public BattleInfo {
public:
    std::vector<std::string> view;  // snapshot rows
    size_t self_x{};                // requesting tank x
    size_t self_y{};                // requesting tank y
    size_t initial_shells{};        // initial shells
};

// -------------------------------------
// Player_322837089_212141253 base
// -------------------------------------
class Player_322837089_212141253 : public Player {
public:
    Player_322837089_212141253(int player_index, size_t width, size_t height,
             size_t max_steps, size_t num_shells);

    void updateTankWithBattleInfo(TankAlgorithm& tank,
                                  SatelliteView& satellite_view) override;

protected:
    int    player_index_{};
    size_t width_{};
    size_t height_{};
    size_t max_steps_{};
    size_t num_shells_{};
    bool   size_known_ = false;
};

// Concrete Player for player 1
class PlayerOne : public Player_322837089_212141253 {
    using Player_322837089_212141253::Player_322837089_212141253;
};

// Concrete Player for player 2
class PlayerTwo : public Player_322837089_212141253 {
    using Player_322837089_212141253::Player_322837089_212141253;
};

// -------------------------------------
// A3 factory (alias): provide a helper to build it
// -------------------------------------
// using PlayerFactory = std::function<std::unique_ptr<Player>(int,size_t,size_t,size_t,size_t)>;
PlayerFactory make_my_player_factory();

}

#endif // Player_322837089_212141253_H
