#include "../common/PlayerRegistration.h"
#include "simulator.h"
#include "AlgorithmRegistrar.h"    //added



PlayerRegistration::PlayerRegistration(PlayerFactory factory) {
    auto f = factory;
    Simulator::getInstance().registerPlayerFactory(std::move(f));  // f instead of factory
    AlgorithmRegistrar::get().addPlayerFactoryToLastEntry(std::move(factory));  // added
}
