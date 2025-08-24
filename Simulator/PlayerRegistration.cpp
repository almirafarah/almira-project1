#include "../common/PlayerRegistration.h"
#include "AlgorithmRegistrar.h"

PlayerRegistration::PlayerRegistration(PlayerFactory factory) {
    // Create a new algorithm entry with a generated name and register via C wrappers
    static int counter = 0;
    std::string name = "Algorithm_" + std::to_string(counter++);
    createAlgorithmFactoryEntry(name.c_str());
    addPlayerFactoryToLastEntry(std::move(factory));
}
