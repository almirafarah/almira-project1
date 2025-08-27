#include "../common/GameManagerRegistration.h"
#include "GameManagerRegistrar.h"
#include "Simulator.h"

GameManagerRegistration::GameManagerRegistration(
    std::function<std::unique_ptr<AbstractGameManager>(bool)> factory) {
    auto f = factory;
    Simulator::getInstance().registerGameManagerFactory(std::move(f));
    auto& registrar = GameManagerRegistrar::get();
    registrar.addGameManagerFactoryToLastEntry(std::move(factory));
}
