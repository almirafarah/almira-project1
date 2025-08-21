#ifndef COMMON_GAMERESULT_H
#define COMMON_GAMERESULT_H

#include <vector>
#include <memory>
#include "SatelliteView.h"

struct GameResult {
	int winner; // 0 = tie
	enum Reason { ALL_TANKS_DEAD, MAX_STEPS, ZERO_SHELLS };
    Reason reason;
    std::vector<size_t> remaining_tanks; // index 0 = player 1, etc.
    std::unique_ptr<SatelliteView> gameState; // at end of game
    size_t rounds; // total number of rounds
    
    // Default constructor
    GameResult() : winner(0), reason(MAX_STEPS), rounds(0) {}
    
    // Copy constructor
    GameResult(const GameResult& other)
        : winner(other.winner)
        , reason(other.reason)
        , remaining_tanks(other.remaining_tanks)
        , gameState(other.gameState ? other.gameState->clone() : nullptr)
        , rounds(other.rounds) {}
    
    // Copy assignment operator
    GameResult& operator=(const GameResult& other) {
        if (this != &other) {
            winner = other.winner;
            reason = other.reason;
            remaining_tanks = other.remaining_tanks;
            gameState = other.gameState ? other.gameState->clone() : nullptr;
            rounds = other.rounds;
        }
        return *this;
    }
    
    // Move constructor
    GameResult(GameResult&& other) noexcept
        : winner(other.winner)
        , reason(other.reason)
        , remaining_tanks(std::move(other.remaining_tanks))
        , gameState(std::move(other.gameState))
        , rounds(other.rounds) {}
    
    // Move assignment operator
    GameResult& operator=(GameResult&& other) noexcept {
        if (this != &other) {
            winner = other.winner;
            reason = other.reason;
            remaining_tanks = std::move(other.remaining_tanks);
            gameState = std::move(other.gameState);
            rounds = other.rounds;
        }
        return *this;
    }
};

#endif // COMMON_GAMERESULT_H
