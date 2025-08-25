#include "Player_Simple_212934582_323964676.h"
#include "TankAlgorithm_Simple_212934582_323964676.h"
#include "../common/PlayerRegistration.h"
#include "../common/TankAlgorithmRegistration.h"

using PlayerSimple = Algorithm_212934582_323964676::Player_Simple_212934582_323964676;
using SimpleAI = Algorithm_212934582_323964676::TankAlgorithm_Simple_212934582_323964676;

extern "C" void initialize_algorithm() {
    PlayerRegistration player_reg([](int player_index, size_t width, size_t height,
                                     size_t max_steps, size_t num_shells) {
        return std::make_unique<PlayerSimple>(player_index, width, height,
                                              max_steps, num_shells);
    });
    TankAlgorithmRegistration algo_reg([](int player_index, int tank_index) {
        return std::make_unique<SimpleAI>(player_index, tank_index);
    });
}
