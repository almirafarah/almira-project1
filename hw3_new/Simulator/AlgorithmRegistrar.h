#pragma once
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <cassert>
#include "../common/TankAlgorithm.h"
#include "../common/Player.h"

// Holds (PlayerFactory, TankAlgorithmFactory) for ONE algorithm .so
class AlgorithmRegistrar {
    class AlgorithmAndPlayerFactories {
        std::string so_name_;
        TankAlgorithmFactory tankAlgorithmFactory_;
        PlayerFactory        playerFactory_;
    public:
        explicit AlgorithmAndPlayerFactories(std::string so_name)
            : so_name_(std::move(so_name)) {}

        void setTankAlgorithmFactory(TankAlgorithmFactory&& f) {
            assert(!tankAlgorithmFactory_);
            tankAlgorithmFactory_ = std::move(f);
        }
        void setPlayerFactory(PlayerFactory&& f) {
            assert(!playerFactory_);
            playerFactory_ = std::move(f);
        }

        const std::string& name() const { return so_name_; }
        bool hasPlayerFactory() const { return (bool)playerFactory_; }
        bool hasTankAlgorithmFactory() const { return (bool)tankAlgorithmFactory_; }

        std::unique_ptr<Player> createPlayer(int player_index,
            size_t x,size_t y,size_t max_steps,size_t num_shells) const
        {
            return playerFactory_(player_index, x, y, max_steps, num_shells);
        }
        std::unique_ptr<TankAlgorithm> createTankAlgorithm(int player_index, int tank_index) const {
            return tankAlgorithmFactory_(player_index, tank_index);
        }
    };

    std::vector<AlgorithmAndPlayerFactories> algorithms_;
    static AlgorithmRegistrar singleton_;

public:
    struct BadRegistrationException {
        std::string name;
        bool hasName;
        bool hasPlayerFactory;
        bool hasTankAlgorithmFactory;
    };

    static AlgorithmRegistrar& get();

    // Called BEFORE dlopen(dylib) for this algorithm .so:
    void createAlgorithmFactoryEntry(const std::string& so_base_name) {
        algorithms_.emplace_back(so_base_name);
    }

    // Called by PlayerRegistration/TankAlgorithmRegistration (from inside the .so)
    void addPlayerFactoryToLastEntry(PlayerFactory&& factory) {
        algorithms_.back().setPlayerFactory(std::move(factory));
    }
    void addTankAlgorithmFactoryToLastEntry(TankAlgorithmFactory&& factory) {
        algorithms_.back().setTankAlgorithmFactory(std::move(factory));
    }

    // Validate AFTER dlopen (and remove on failure)
    void validateLastRegistration() {
        const auto& last = algorithms_.back();
        bool hasName = !last.name().empty();
        if (!hasName || !last.hasPlayerFactory() || !last.hasTankAlgorithmFactory()) {
            throw BadRegistrationException{
                .name = last.name(),
                .hasName = hasName,
                .hasPlayerFactory = last.hasPlayerFactory(),
                .hasTankAlgorithmFactory = last.hasTankAlgorithmFactory()
            };
        }
    }
    void removeLast() { algorithms_.pop_back(); }

    // Iteration
    auto begin() const { return algorithms_.begin(); }
    auto end()   const { return algorithms_.end();   }
    std::size_t count() const { return algorithms_.size(); }
    void clear() { algorithms_.clear(); }
};
