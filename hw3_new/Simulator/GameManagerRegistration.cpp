#include "../common/GameManagerRegistration.h"
#include "GameManagerRegistrar.h"
#include "simulator.h"

GameManagerRegistration::GameManagerRegistration(
    std::function<std::unique_ptr<AbstractGameManager>(bool verbose)> factory)
{
    auto f = factory;
    Simulator::getInstance().registerGameManagerFactory(std::move(f));
    auto& registrar = GameManagerRegistrar::get();
    registrar.addGameManagerFactoryToLastEntry(std::move(factory));
}
