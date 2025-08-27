#include "../common/TankAlgorithmRegistration.h"
#include "simulator.h"
#include "AlgorithmRegistrar.h"   //added

TankAlgorithmRegistration::TankAlgorithmRegistration(TankAlgorithmFactory factory) {
    auto f = factory; // added
    Simulator::getInstance().registerTankAlgorithmFactory(std::move(f)); // f instead of factory
    AlgorithmRegistrar::get().addTankAlgorithmFactoryToLastEntry(std::move(factory)); // added
}
