#include "../common/TankAlgorithmRegistration.h"
#include "AlgorithmRegistrar.h"

TankAlgorithmRegistration::TankAlgorithmRegistration(TankAlgorithmFactory factory) {
    auto& registrar = AlgorithmRegistrar::getAlgorithmRegistrar();

    // Add the TankAlgorithm factory to the last entry (created by PlayerRegistration)
    registrar.addTankAlgorithmFactoryToLastEntry(std::move(factory));

    // Validate that both Player and TankAlgorithm factories are present
    registrar.validateLastRegistration();
}
