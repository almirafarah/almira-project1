#include <iostream>
#include <memory>
#include "../Simulator/AlgorithmRegistrar.h"
#include "Player_Aggressive_212934582_323964676.h"
#include "AggressiveTankAI_212934582_323964676.h"

using namespace Algorithm_212934582_323964676;

extern "C" {
    // Register directly into the simulator's registrar instance passed from the host
    void register_algorithms_aggressive_212934582_323964676(AlgorithmRegistrar* registrar) {
        std::cout << "[DEBUG] Aggressive DLL register()" << std::endl;
        registrar->createAlgorithmFactoryEntry("Aggressive_212934582_323964676");
        registrar->addPlayerFactoryToLastEntry(
            [](int player_index, size_t width, size_t height, size_t max_steps, size_t num_shells) {
                return std::make_unique<Player_Aggressive_212934582_323964676>(
                    player_index, width, height, max_steps, num_shells);
            }
        );
        registrar->addTankAlgorithmFactoryToLastEntry(
            [](int player_index, int tank_index) {
                return std::make_unique<AggressiveTankAI_212934582_323964676>(player_index, tank_index);
            }
        );
        registrar->validateLastRegistration();
        std::cout << "[DEBUG] Aggressive registered into host registrar" << std::endl;
    }
}
