#ifndef TANKALGORITHM_SIMPLE_212934582_323964676_H
#define TANKALGORITHM_SIMPLE_212934582_323964676_H

#include "../common/TankAlgorithm.h"
#include "../common/BattleInfo.h"
#include "../common/ActionRequest.h"
#include <memory>

namespace Algorithm_212934582_323964676 {

class TankAlgorithm_Simple_212934582_323964676 : public TankAlgorithm {
private:
    int player_index_;
    int tank_index_;
    std::unique_ptr<BattleInfo> current_battle_info_;
    
public:
    TankAlgorithm_Simple_212934582_323964676(int player_index, int tank_index);
    ActionRequest getAction() override;
    void updateBattleInfo(BattleInfo& info) override;
};

} // namespace Algorithm_212934582_323964676

#endif // TANKALGORITHM_SIMPLE_212934582_323964676_H
