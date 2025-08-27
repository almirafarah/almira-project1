#pragma once
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <cassert>
#include "../common/AbstractGameManager.h"

// Holds the AbstractGameManager factory for ONE GM .so
class GameManagerRegistrar {
    class GMEntry {
        std::string so_name_;
        std::function<std::unique_ptr<AbstractGameManager>(bool verbose)> factory_;
    public:
        explicit GMEntry(std::string so_name) : so_name_(std::move(so_name)) {}
        void setFactory(std::function<std::unique_ptr<AbstractGameManager>(bool)> f) {
            assert(!factory_);
            factory_ = std::move(f);
        }
        const std::string& name() const { return so_name_; }
        bool hasFactory() const { return (bool)factory_; }
        std::unique_ptr<AbstractGameManager> create(bool verbose) const {
            return factory_(verbose);
        }
    };

    std::vector<GMEntry> managers_;
    static GameManagerRegistrar singleton_;

public:
    struct BadRegistrationException {
        std::string name;
        bool hasName;
        bool hasFactory;
    };

    static GameManagerRegistrar& get();

    // BEFORE dlopen of this GM .so
    void createGameManagerEntry(const std::string& so_base_name) {
        managers_.emplace_back(so_base_name);
    }

    // Called by GameManagerRegistration (from inside the GM .so)
    void addGameManagerFactoryToLastEntry(std::function<std::unique_ptr<AbstractGameManager>(bool)> f) {
        managers_.back().setFactory(std::move(f));
    }

    // Validate AFTER dlopen
    void validateLastRegistration() {
        const auto& last = managers_.back();
        bool hasName = !last.name().empty();
        if (!hasName || !last.hasFactory()) {
            throw BadRegistrationException{
                .name = last.name(),
                .hasName = hasName,
                .hasFactory = last.hasFactory()
            };
        }
    }
    void removeLast() { managers_.pop_back(); }

    // Iteration
    auto begin() const { return managers_.begin(); }
    auto end()   const { return managers_.end();   }
    std::size_t count() const { return managers_.size(); }
    void clear() { managers_.clear(); }
};