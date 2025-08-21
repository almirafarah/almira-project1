#include "../common/GameManagerRegistration.h"
#include <map>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Global registry for GameManager factories
static std::map<std::string, GameManagerFactory> game_manager_registry;

GameManagerRegistration::GameManagerRegistration(GameManagerFactory factory) {
    // For now, we'll use a simple approach - store the factory with a default name
    // In a real implementation, you might want to extract the class name from the factory
    static int counter = 0;
    std::string name = "GameManager_" + std::to_string(counter++);
    game_manager_registry[name] = factory;
}

// Function to get a GameManager factory by name
GameManagerFactory getGameManagerFactory(const std::string& name) {
    auto it = game_manager_registry.find(name);
    if (it != game_manager_registry.end()) {
        return it->second;
    }
    return nullptr;
}

// Function to list all registered GameManagers
std::vector<std::string> getRegisteredGameManagers() {
    std::vector<std::string> names;
    for (const auto& [name, _] : game_manager_registry) {
        names.push_back(name);
    }
    return names;
}
