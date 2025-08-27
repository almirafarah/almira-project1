#ifndef TankAlgorithm_322837089_212141253_H
#define TankAlgorithm_322837089_212141253_H

#include "../common/ActionRequest.h"
#include "../common/TankAlgorithm.h"
#include "../common/SatelliteView.h"
#include "../common/TankAlgorithmRegistration.h"
#include "Player_322837089_212141253.h"                 // <- MyBattleInfo now lives here
#include <memory>
#include <vector>
#include <string>

namespace Algorithm_322837089_212141253 {

// Local helper types
struct Position1 {
    int x, y;
    bool operator==(const Position1& other) const { return x == other.x && y == other.y; }
};

inline bool isBlocking(char c) {
    return c == '#' || c == '@' || c == '1' || c == '2';
}

// ---------------------------------------------
// TankAlgorithm_322837089_212141253  (Attacker)
// ---------------------------------------------
class TankAlgorithm_322837089_212141253 : public TankAlgorithm {
public:
    TankAlgorithm_322837089_212141253(int player_index, int tank_index);
    ActionRequest getAction() override;
    void updateBattleInfo(BattleInfo& info) override;
    void setInitialState(int orientation);

private:
    int playerIndex;
    int tankIndex;
    int posX, posY;
    int orientation;         // 0..7
    int ammo;
    int shootCooldown;
    int backwardWait;
    bool infoReceived;
    std::vector<std::string> staticMap;
    int mapWidth, mapHeight;
    std::vector<Position1> currentPath;
    void computePath();
    int turnsSinceInfo = 0;
};

// ---------------------------------------------
// A3 factory helper (returns the alias)
// using TankAlgorithmFactory =
//   std::function<std::unique_ptr<TankAlgorithm>(int,int)>;
// ---------------------------------------------
TankAlgorithmFactory make_my_tank_algorithm_factory();

}

#endif // TankAlgorithm_322837089_212141253_H
