#include "../common/PlayerRegistration.h"
#include "Simulator.h"
#include "AlgorithmRegistrar.h"

PlayerRegistration::PlayerRegistration(PlayerFactory factory) {
    auto f = factory;
    Simulator::getInstance().registerPlayerFactory(std::move(f));
    AlgorithmRegistrar::get().addPlayerFactoryToLastEntry(std::move(factory));
}
