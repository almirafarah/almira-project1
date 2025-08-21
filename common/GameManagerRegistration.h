#ifndef COMMON_GAMEMANAGERREGISTRATION_H
#define COMMON_GAMEMANAGERREGISTRATION_H

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "AbstractGameManager.h"

using GameManagerFactory = std::function<std::unique_ptr<AbstractGameManager>(bool verbose)>;

struct GameManagerRegistration {
  GameManagerRegistration(GameManagerFactory);
};

// Function declarations for accessing registered GameManagers
GameManagerFactory getGameManagerFactory(const std::string& name);
std::vector<std::string> getRegisteredGameManagers();

#define REGISTER_GAME_MANAGER(class_name) \
GameManagerRegistration register_me_##class_name \
        ( [] (bool verbose) { return std::make_unique<class_name>(verbose); } );

#endif // COMMON_GAMEMANAGERREGISTRATION_H
