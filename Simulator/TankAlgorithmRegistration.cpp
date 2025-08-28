#include "../common/TankAlgorithmRegistration.h"
#include "Simulator.h"
#include "AlgorithmRegistrar.h"

TankAlgorithmRegistration::TankAlgorithmRegistration(TankAlgorithmFactory factory) {
    auto f = factory;
    Simulator::getInstance().registerTankAlgorithmFactory(std::move(f));
    AlgorithmRegistrar::get().addTankAlgorithmFactoryToLastEntry(std::move(factory));
}
