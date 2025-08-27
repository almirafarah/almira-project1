#pragma once

#include <functional>
#include <memory>
#include "AbstractGameManager.h"  // <-- brings in GameManagerFactory alias

// GameManagerFactory is defined in AbstractGameManager.h as:
// using GameManagerFactory =
//   std::function<std::unique_ptr<AbstractGameManager>(bool verbose)>;

struct GameManagerRegistration {
    explicit GameManagerRegistration(GameManagerFactory factory);
};

// Use with a trailing semicolon where you place it:
#define REGISTER_GAME_MANAGER(class_name)                                       \
    GameManagerRegistration register_me_##class_name(                           \
        [](bool verbose) { return std::make_unique<class_name>(verbose); } )
