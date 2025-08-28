#include "../common/TankAlgorithmRegistration.h"
#include "AlgorithmRegistrar.h"

TankAlgorithmRegistration::TankAlgorithmRegistration(TankAlgorithmFactory factory) {
    AlgorithmRegistrar::get().addTankAlgorithmFactoryToLastEntry(std::move(factory));
}
