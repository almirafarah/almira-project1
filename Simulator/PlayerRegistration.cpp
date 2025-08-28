#include "../common/PlayerRegistration.h"
#include "AlgorithmRegistrar.h"

PlayerRegistration::PlayerRegistration(PlayerFactory factory) {
    AlgorithmRegistrar::get().addPlayerFactoryToLastEntry(std::move(factory));
}
