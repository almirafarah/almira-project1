// Centralized registration to guarantee deterministic init order per algorithm
#include "Player_Aggressive_212934582_323964676.h"
#include "Player_Simple_212934582_323964676.h"
#include "AggressiveTankAI_212934582_323964676.h"
#include "TankAlgorithm_Simple_212934582_323964676.h"
#include "../common/PlayerRegistration.h"
#include "../common/TankAlgorithmRegistration.h"

// Algorithm A: Aggressive (HW2 BFS-based)
using PlayerTypeA = Algorithm_212934582_323964676::Player_Aggressive_212934582_323964676;
REGISTER_PLAYER(PlayerTypeA)
using TankAlgorithmTypeA = Algorithm_212934582_323964676::AggressiveTankAI_212934582_323964676;
REGISTER_TANK_ALGORITHM(TankAlgorithmTypeA)

// Algorithm B: Simple/defensive
using PlayerTypeB = Algorithm_212934582_323964676::Player_Simple_212934582_323964676;
REGISTER_PLAYER(PlayerTypeB)
using TankAlgorithmTypeB = Algorithm_212934582_323964676::TankAlgorithm_Simple_212934582_323964676;
REGISTER_TANK_ALGORITHM(TankAlgorithmTypeB)

// Force initialization function to ensure static variables are initialized
extern "C" int force_registration_initialization() {
    // This function exists to ensure this compilation unit is linked
    // and the static registration variables above are initialized
    return 42;
}



