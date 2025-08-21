#include "TankAlgorithm_Simple_212934582_323964676.h"
#include "../common/BattleInfo.h"
#include "../common/ActionRequest.h"
#include <random>
#include <iostream>

namespace Algorithm_212934582_323964676 {

TankAlgorithm_Simple_212934582_323964676::TankAlgorithm_Simple_212934582_323964676(int player_index, int tank_index)
    : player_index_(player_index), tank_index_(tank_index) {
}

ActionRequest TankAlgorithm_Simple_212934582_323964676::getAction() {
    // Defensive/evasive algorithm that prioritizes movement and survival
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 10);
    
    int action = dis(gen);
    
    // 40% chance to move (actions 0, 1, 2, 3 = MoveForward, MoveBackward, RotateLeft90, RotateRight90)
    if (action <= 3) {
        static std::uniform_int_distribution<> move_dis(0, 3);  // MoveForward, MoveBackward, RotateLeft90, RotateRight90
        return static_cast<ActionRequest>(move_dis(gen));
    }
    // 30% chance to rotate for better positioning (actions 4, 5, 6 = rotate)
    else if (action <= 6) {
        static std::uniform_int_distribution<> rotate_dis(2, 5);  // RotateLeft90, RotateRight90, RotateLeft45, RotateRight45
        return static_cast<ActionRequest>(rotate_dis(gen));
    }
    // 20% chance to shoot (actions 7, 8 = Shoot)
    else if (action <= 8) {
        return ActionRequest::Shoot;
    }
    // 10% GetBattleInfo (action 9)
    else if (action == 9) {
        return ActionRequest::GetBattleInfo;
    }
    // 10% DoNothing (action 10)
    else {
        return ActionRequest::DoNothing;
    }
}

void TankAlgorithm_Simple_212934582_323964676::updateBattleInfo(BattleInfo& info) {
    // Store the battle info for decision making
    current_battle_info_ = std::make_unique<BattleInfo>(info);
}

} // namespace Algorithm_212934582_323964676


