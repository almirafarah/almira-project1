#ifndef PLAYER_SIMPLE_212934582_323964676_H
#define PLAYER_SIMPLE_212934582_323964676_H

#include "../common/Player.h"
#include "../common/TankAlgorithm.h"
#include "../common/SatelliteView.h"

namespace Algorithm_212934582_323964676 {

class Player_Simple_212934582_323964676 : public Player {
public:
    Player_Simple_212934582_323964676(int player_index, size_t x, size_t y, size_t max_steps, size_t num_shells);
    virtual ~Player_Simple_212934582_323964676() = default;

    // Player interface
    virtual void updateTankWithBattleInfo(TankAlgorithm& tank, SatelliteView& satellite_view) override;

private:
    int player_index_;
    size_t x_, y_;
    size_t max_steps_, num_shells_;
};

} // namespace Algorithm_212934582_323964676

#endif // PLAYER_SIMPLE_212934582_323964676_H
