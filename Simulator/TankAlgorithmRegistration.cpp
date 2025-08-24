#include "../common/TankAlgorithmRegistration.h"
#include "AlgorithmRegistrar.h"

TankAlgorithmRegistration::TankAlgorithmRegistration(TankAlgorithmFactory factory) {
    // Add the TankAlgorithm factory to the last entry (created by PlayerRegistration)
    addTankAlgorithmFactoryToLastEntry(std::move(factory));

    // Validate that both Player and TankAlgorithm factories are present
    validateLastRegistration();
}
