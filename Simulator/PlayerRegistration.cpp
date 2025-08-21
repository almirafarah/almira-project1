#include "../common/PlayerRegistration.h"
#include "AlgorithmRegistrar.h"

PlayerRegistration::PlayerRegistration(PlayerFactory factory) {
    auto& registrar = AlgorithmRegistrar::getAlgorithmRegistrar();

    // Create a new algorithm entry with a generated name
    static int counter = 0;
    std::string name = "Algorithm_" + std::to_string(counter++);

    registrar.createAlgorithmFactoryEntry(name);
    registrar.addPlayerFactoryToLastEntry(std::move(factory));
}
