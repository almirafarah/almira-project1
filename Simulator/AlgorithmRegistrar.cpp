#include "AlgorithmRegistrar.h"

AlgorithmRegistrar AlgorithmRegistrar::registrar;

AlgorithmRegistrar& AlgorithmRegistrar::getAlgorithmRegistrar() {
    return registrar;
}

extern "C" void createAlgorithmFactoryEntry(const char* name) {
    AlgorithmRegistrar::getAlgorithmRegistrar().createAlgorithmFactoryEntry(name);
}

extern "C" void addPlayerFactoryToLastEntry(PlayerFactory factory) {
    AlgorithmRegistrar::getAlgorithmRegistrar().addPlayerFactoryToLastEntry(std::move(factory));
}

extern "C" void addTankAlgorithmFactoryToLastEntry(TankAlgorithmFactory factory) {
    AlgorithmRegistrar::getAlgorithmRegistrar().addTankAlgorithmFactoryToLastEntry(std::move(factory));
}

extern "C" void validateLastRegistration() {
    AlgorithmRegistrar::getAlgorithmRegistrar().validateLastRegistration();
}
