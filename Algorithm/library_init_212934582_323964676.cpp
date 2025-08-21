#include <iostream>
#include "../common/PlayerRegistration.h"
#include "../common/TankAlgorithmRegistration.h"
#include "../Simulator/AlgorithmRegistrar.h"
#include "Player_Aggressive_212934582_323964676.h"
#include "Player_Simple_212934582_323964676.h"
#include "AggressiveTankAI_212934582_323964676.h"
#include "TankAlgorithm_Simple_212934582_323964676.h"

using namespace Algorithm_212934582_323964676;

extern "C" {
    /**
     * Initialization function called when the Algorithm library is dynamically loaded.
     * This manually triggers the registrations since static constructors may not work in dynamic libraries.
     */
    void initialize_algorithm_212934582_323964676() {
        std::cout << "[DEBUG] Algorithm initialization function called!" << std::endl;
        
        // Manually register players and algorithms in correct order
        // Each Player registration must be immediately followed by its TankAlgorithm
        
        std::cout << "[DEBUG] Registering Player_Aggressive + AggressiveTankAI..." << std::endl;
        static PlayerRegistration player_aggressive_reg([](int player_index, size_t width, size_t height, size_t max_steps, size_t num_shells) {
            return std::make_unique<Player_Aggressive_212934582_323964676>(player_index, width, height, max_steps, num_shells);
        });
        static TankAlgorithmRegistration algorithm_aggressive_reg([](int player_index, int tank_index) {
            return std::make_unique<AggressiveTankAI_212934582_323964676>(player_index, tank_index);
        });
        
        std::cout << "[DEBUG] Registering Player_Simple + TankAlgorithm_Simple..." << std::endl;
        static PlayerRegistration player_simple_reg([](int player_index, size_t width, size_t height, size_t max_steps, size_t num_shells) {
            return std::make_unique<Player_Simple_212934582_323964676>(player_index, width, height, max_steps, num_shells);
        });
        static TankAlgorithmRegistration algorithm_simple_reg([](int player_index, int tank_index) {
            return std::make_unique<TankAlgorithm_Simple_212934582_323964676>(player_index, tank_index);
        });
        
        // Debug: Check what got registered
        std::cout << "[DEBUG] Algorithm initialization completed!" << std::endl;
        
        // Let's check the AlgorithmRegistrar directly
        auto& registrar = AlgorithmRegistrar::getAlgorithmRegistrar();
        std::cout << "[DEBUG] AlgorithmRegistrar count: " << registrar.count() << std::endl;
    }
}
