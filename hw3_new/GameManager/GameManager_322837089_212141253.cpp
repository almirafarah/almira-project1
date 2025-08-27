#include "GameManager_322837089_212141253.h"
#include <cctype>
#include <fstream>
#include <iostream>

using namespace GameManager_322837089_212141253;

// Directions for orientations 0..7: (U, UR, R, DR, D, DL, L, UL)
static const int dirDX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int dirDY[8] = {-1,-1, 0, 1, 1,  1,  0, -1};

static inline bool isBlockingCell(char c) { return c=='#' || c=='$' || c=='@'; }

GameManager::GameManager(bool verbose)
    : verbose_(verbose) {}

// ---- SatelliteViewImpl (live view for GetBattleInfo) ----
GameManager::SatelliteViewImpl::SatelliteViewImpl(
    const GameManager& g, int idx, int asking_player)
: gm(g), requestingTankIndex(idx), asking_player(asking_player) {}

char GameManager::SatelliteViewImpl::getObjectAt(size_t x,size_t y) const
{
    if (x >= (size_t)gm.cols || y >= (size_t)gm.rows) return '&';
    char base = gm.board[y][x];
    if (base!='#' && base!='@' && base!=' ' && base!='$') base=' ';

    // Tanks
    for (size_t i=0;i<gm.tanks.size();++i)
        if (gm.tanks[i].alive && gm.tanks[i].x==(int)x && gm.tanks[i].y==(int)y) {
            // compute the index of this tank among "asking_player" tanks
            int id = 0;
            for (size_t j=0; j<i; ++j)
                if (gm.tanks[j].player == asking_player) id++;
            return (id==requestingTankIndex && gm.tanks[i].player==asking_player) ? '%'
                   : (gm.tanks[i].player==1 ? '1' : '2');
        }

    if (base == '$') base = '#'; // weakened wall is seen as wall in snapshot
    return base;
}

// ---- helpers ----
bool GameManager::occupied(int x, int y) const {
    for (const auto& t : tanks) if (t.alive && t.x==x && t.y==y) return true;
    return false;
}

void GameManager::resolveCollisions(
    std::vector<Position>& prevTankPos)
{
    // 1) shell hits wall
    for (auto& sh : shells)
        if (sh.live && (board[sh.y][sh.x]=='#' || board[sh.y][sh.x]=='$')) {
            sh.live = false;
            if (board[sh.y][sh.x] == '#')      board[sh.y][sh.x] = '$'; // weaken
            else if (board[sh.y][sh.x] == '$') board[sh.y][sh.x] = ' '; // remove
        }

    // 2) prepare hit maps
    std::vector<bool> tankHit (tanks .size(), false);
    std::vector<bool> shellHit(shells.size(), false);

    // 3) tank steps on mine
    for (size_t i=0;i<tanks.size();++i)
        if (tanks[i].alive && board[tanks[i].y][tanks[i].x]=='@') {
            tankHit[i] = true;
            board[tanks[i].y][tanks[i].x] = ' ';
        }

    // 4) gather live objs
    struct Obj { bool tank; size_t idx; int x,y,px,py; };
    std::vector<Obj> objs; objs.reserve(tanks.size()+shells.size());
    for (size_t i=0;i<tanks.size();++i)
        if (tanks[i].alive)
            objs.push_back({true,i,tanks[i].x,tanks[i].y,prevTankPos[i].x,prevTankPos[i].y});
    for (size_t i=0;i<shells.size();++i)
        if (shells[i].live)
            objs.push_back({false,i,shells[i].x,shells[i].y,shells[i].prevX,shells[i].prevY});

    // 5) collisions: same cell OR crossing
    for (size_t i=0;i<objs.size();++i)
        for (size_t j=i+1;j<objs.size();++j) {
            bool same  = objs[i].x==objs[j].x && objs[i].y==objs[j].y;
            bool cross = objs[i].x==objs[j].px && objs[i].y==objs[j].py &&
                         objs[j].x==objs[i].px && objs[j].y==objs[i].py;
            if (!same && !cross) continue;

            if (objs[i].tank && objs[j].tank) {
                tankHit [objs[i].idx] = true;
                tankHit [objs[j].idx] = true;
            } else if (objs[i].tank && !objs[j].tank) {
                tankHit  [objs[i].idx] = true;
                shellHit [objs[j].idx] = true;
            } else if (!objs[i].tank && objs[j].tank) {
                shellHit [objs[i].idx] = true;
                tankHit  [objs[j].idx] = true;
            } else {
                shellHit[objs[i].idx] = true;
                shellHit[objs[j].idx] = true;
            }
        }

    // 6) apply deaths
    for (size_t i=0;i<tanks.size();++i) if (tankHit [i]) tanks [i].alive = false;
    for (size_t i=0;i<shells.size();++i) if (shellHit[i]) shells[i].live  = false;
}

// ---- main run ----
GameResult GameManager::run(
    size_t map_width, size_t map_height,
    const SatelliteView& map, std::string map_name,
    size_t max_steps, size_t num_shells,
    Player& player1, std::string /*name1*/,
    Player& player2, std::string /*name2*/,
    TankAlgorithmFactory player1_taf,
    TankAlgorithmFactory player2_taf)
{
    // initialize run params
    cols = (int)map_width;
    rows = (int)map_height;
    maxSteps = (int)max_steps;
    initialShells = (int)num_shells;
    rounds_without_shells = 0;
    std::ofstream fout;
    auto sanitize = [](std::string s){ for (auto& ch : s) if (ch=='/'||ch=='\\') ch = '_'; return s; };
    std::cout << "verbose flage is: " << verbose_<< "\n";
    if (verbose_) {
        fout.open("output_" + sanitize(map_name) + ".txt");
    }
    shells.clear();
    tanks.clear();

    p1_ = &player1;
    p2_ = &player2;
    taf1_ = std::move(player1_taf);
    taf2_ = std::move(player2_taf);

    // build initial board from snapshot
    board.assign(rows, std::string(cols,' '));
    for (int y=0;y<rows;++y)
        for (int x=0;x<cols;++x)
            board[y][x] = map.getObjectAt((size_t)x,(size_t)y);
    
    // --- print initial map once (immediately after reading it) ---
        if (verbose_ && fout) {
            for (int y = 0; y < rows; ++y) {
                fout << board[y] << '\n';
            }
            fout.flush();
        }


    // create tanks by scanning board
    int t1 = 0, t2 = 0;
    for (int y=0;y<rows;++y)
        for (int x=0;x<cols;++x) {
            char c = board[y][x];
            if (c=='1' || c=='2') {
                int player = (c=='1'?1:2);
                int idx    = (player==1? t1++ : t2++);
                int orient = (player==1?6:2); // L or R

                // pick correct per-player factory
                std::unique_ptr<TankAlgorithm> alg =
                    (player==1) ? taf1_(1, idx) : taf2_(2, idx);

                TankState ts;
                ts.player=player; ts.index=idx; ts.x=x; ts.y=y;
                ts.orientation=orient; ts.shells=initialShells;
                ts.alive=true; ts.shootCooldown=0;
                ts.backwardWait=0; ts.lastActionWasBackward=false;
                ts.algorithm=std::move(alg);
                tanks.push_back(std::move(ts));
            } else if (c!='#' && c!='$' && c!='@' && c!=' ') {
                // sanitize weird chars to space
                board[y][x] = ' ';
            }
        }

    // if someone has 0 tanks from the start, end immediately
    if (t1==0 || t2==0) {
        GameResult res{};
        if (t1==0 && t2==0) { res.winner = 0; res.reason = GameResult::ALL_TANKS_DEAD; res.remaining_tanks = {0,0}; }
        else if (t1==0)     { res.winner = 2; res.reason = GameResult::ALL_TANKS_DEAD; res.remaining_tanks = {0,(size_t)t2}; }
        else                { res.winner = 1; res.reason = GameResult::ALL_TANKS_DEAD; res.remaining_tanks = {(size_t)t1,0}; }
        res.rounds = 0;
        // final snapshot is just current board with tanks already present
        res.gameState = std::make_unique<SnapshotSatelliteView>(board);
            if (verbose_ && fout) {
    if (t1==0 && t2==0) {
        fout << "Tie, both players have zero tanks\n";
    } else if (t1==0) {
        fout << "Player 2 won with " << t2 << " tanks still alive\n";
    } else {
        fout << "Player 1 won with " << t1 << " tanks still alive\n";
    }
    fout << 0 << "\n";
    // final board snapshot with alive tanks
    std::vector<std::string> finalGrid = board;
    for (const auto& t : tanks) if (t.alive) {
        finalGrid[t.y][t.x] = (t.player==1 ? '1' : '2');
    }
    for (const auto& row : finalGrid) fout << row << "\n";
}


        return res;
    }

    // main loop
    
    int step = 0;
    bool gameEnded = false;
    while (step < maxSteps && !gameEnded) {
        ++step;
        
        std::vector<std::string> actStr(tanks.size(), "killed");

        auto nameOf = [&](ActionRequest r) -> const char* {
            switch (r) {
                case ActionRequest::MoveForward:  return "MoveForward";
                case ActionRequest::MoveBackward: return "MoveBackward";
                case ActionRequest::RotateLeft90: return "RotateLeft90";
                case ActionRequest::RotateRight90:return "RotateRight90";
                case ActionRequest::RotateLeft45: return "RotateLeft45";
                case ActionRequest::RotateRight45:return "RotateRight45";
                case ActionRequest::Shoot:        return "Shoot";
                case ActionRequest::GetBattleInfo:return "GetBattleInfo";
                default:                          return "DoNothing";
            }
        };

        // Shell step A
        for (auto& sh : shells) if (sh.live) {
            sh.prevX = sh.x; sh.prevY = sh.y;
            sh.x = (sh.x + sh.dx + cols) % cols;
            sh.y = (sh.y + sh.dy + rows) % rows;
        }

        // save previous tank positions (for crossing collisions)
        std::vector<Position> tankPrev(tanks.size());
        for (size_t i=0;i<tanks.size();++i) tankPrev[i] = {tanks[i].x, tanks[i].y};
       
        resolveCollisions(tankPrev);
     

        // remove dead shells
        shells.erase(
            std::remove_if(shells.begin(), shells.end(), [](const Bullet& b){ return !b.live; }),
            shells.end()
        );

        // collect actions
        struct MoveInfo { bool want=false; int tx{}, ty{}; bool valid=true; };
        std::vector<MoveInfo> move(tanks.size());
        
        
        for (size_t i=0;i<tanks.size();++i) {
            if (!tanks[i].alive) continue;
            ActionRequest req = tanks[i].algorithm->getAction();
            //std::cout << "action is: "<< nameOf(req) << "\n";
            std::string label = nameOf(req); 
            if (tanks[i].backwardWait > 0) {
                if (req == ActionRequest::MoveForward) {
                    tanks[i].backwardWait = 0;
                    label = "MoveForward";
                } else {
                    label += " (ignored)";
                }
                actStr[i] = label;
                continue;
            }


            if (req == ActionRequest::GetBattleInfo) {
                //SatelliteViewImpl view(*this, (int)i, tanks[i].player);
                SatelliteViewImpl view(*this, tanks[i].index, tanks[i].player);
                if (tanks[i].player == 1) p1_->updateTankWithBattleInfo(*tanks[i].algorithm, view);
                else                      p2_->updateTankWithBattleInfo(*tanks[i].algorithm, view);
                actStr[i] = "GetBattleInfo";
                continue;
            }

            if (req == ActionRequest::RotateLeft45)  tanks[i].orientation = (tanks[i].orientation + 7) % 8;
            if (req == ActionRequest::RotateRight45) tanks[i].orientation = (tanks[i].orientation + 1) % 8;
            if (req == ActionRequest::RotateLeft90)  tanks[i].orientation = (tanks[i].orientation + 6) % 8;
            if (req == ActionRequest::RotateRight90) tanks[i].orientation = (tanks[i].orientation + 2) % 8;

            if (req == ActionRequest::RotateLeft45)       label = "RotateLeft45";   //added from here
            else if (req == ActionRequest::RotateRight45) label = "RotateRight45";
            else if (req == ActionRequest::RotateLeft90)  label = "RotateLeft90";
            else if (req == ActionRequest::RotateRight90) label = "RotateRight90";  //to here


            /*if (req == ActionRequest::Shoot) {
                if (tanks[i].shells > 0 && tanks[i].shootCooldown == 0) {
                    Bullet b{tanks[i].x, tanks[i].y, tanks[i].x, tanks[i].y,
                             dirDX[tanks[i].orientation], dirDY[tanks[i].orientation], true};
                    shells.push_back(b);
                    tanks[i].shells--;
                    tanks[i].shootCooldown = 4;
                }
            }*/

            if (req == ActionRequest::Shoot) {
                if (tanks[i].shells > 0 && tanks[i].shootCooldown == 0) {
                    Bullet b{tanks[i].x, tanks[i].y, tanks[i].x, tanks[i].y,
                         dirDX[tanks[i].orientation], dirDY[tanks[i].orientation], true};
                    shells.push_back(b);
                    tanks[i].shells--;
                    tanks[i].shootCooldown = 4;
                    label = "Shoot";
                } else {
                    label = "Shoot (ignored)";
                }
            }


            if (req == ActionRequest::MoveForward || req == ActionRequest::MoveBackward) {
                int dx = dirDX[tanks[i].orientation], dy = dirDY[tanks[i].orientation];
                if (req == ActionRequest::MoveBackward) { dx = -dx; dy = -dy; }
                int tx = (tanks[i].x + dx + cols) % cols;
                int ty = (tanks[i].y + dy + rows) % rows;
                move[i] = {true, tx, ty, true};
                label = (req == ActionRequest::MoveForward) ? "MoveForward" : "MoveBackward";
                if (req == ActionRequest::MoveBackward && !tanks[i].lastActionWasBackward)
                    tanks[i].backwardWait = 2;
            }
            actStr[i] = label;
        }

        // invalidate moves into walls or occupied cells
        for (size_t i=0;i<tanks.size();++i) {
            if (!tanks[i].alive || !move[i].want) continue;
            if (isBlockingCell(board[move[i].ty][move[i].tx]) || occupied(move[i].tx, move[i].ty)) {
                move[i].valid = false;
            }
        }

        // prevent swap moves
        for (size_t a=0; a<tanks.size(); ++a) if (move[a].want && move[a].valid)
            for (size_t b=a+1; b<tanks.size(); ++b)
                if (move[b].want && move[b].valid &&
                    move[a].tx == tanks[b].x && move[a].ty == tanks[b].y &&
                    move[b].tx == tanks[a].x && move[b].ty == tanks[a].y) {
                    move[a].valid = move[b].valid = false;
                }
        for (size_t i=0;i<tanks.size();++i) {
            if (move[i].want && !move[i].valid) actStr[i] += " (ignored)";
        }
        // apply valid moves
        for (size_t i=0;i<tanks.size();++i)
            if (move[i].want && move[i].valid) { tanks[i].x = move[i].tx; tanks[i].y = move[i].ty; }

        // cooldowns + lastActionWasBackward
        for (size_t i=0;i<tanks.size();++i) {
            auto& t = tanks[i];
            if (t.shootCooldown > 0) t.shootCooldown--;
            t.lastActionWasBackward = move[i].want && ( ( (t.x - tankPrev[i].x + cols) % cols != 0) ||
                                                        ( (t.y - tankPrev[i].y + rows) % rows != 0) );
        }

        // Shell step B
        for (auto& sh : shells) if (sh.live) {
            sh.prevX = sh.x; sh.prevY = sh.y;
            sh.x = (sh.x + sh.dx + cols) % cols;
            sh.y = (sh.y + sh.dy + rows) % rows;
        }

        std::vector<Position> tankPrev2(tanks.size());
        for (size_t i=0;i<tanks.size();++i) tankPrev2[i] = {tanks[i].x, tanks[i].y};
        resolveCollisions(tankPrev2);

        shells.erase(
            std::remove_if(shells.begin(), shells.end(), [](const Bullet& b){ return !b.live; }),
            shells.end()
        );

        // append (killed) to any tank that died this step
        for (size_t i=0;i<tanks.size();++i) {
            if (!tanks[i].alive && actStr[i] != "killed" &&
                actStr[i].find("(killed)") == std::string::npos) {
                actStr[i] += " (killed)";
            }
        }

        // write the per-step actions line
        if (verbose_ && fout) {
            for (size_t i=0;i<actStr.size();++i) {
                fout << actStr[i];
                if (i + 1 < actStr.size()) fout << ", ";
            }
            fout << '\n';
            fout.flush();
        }

        // check end conditions
        int alive1 = 0, alive2 = 0;
        bool allAliveTanksOutOfShells = true;
        for (auto& t : tanks) if (t.alive) {
            if (t.shells > 0) allAliveTanksOutOfShells = false;
            if (t.player == 1) ++alive1; else ++alive2;
        }

        if (alive1 == 0 && alive2 == 0) { gameEnded = true; break; }
        if (alive1 == 0 || alive2 == 0) { gameEnded = true; break; }

        if (allAliveTanksOutOfShells) ++rounds_without_shells;
        else                           rounds_without_shells = 0;

        if (rounds_without_shells >= rounds_without_shells_limit) {
            gameEnded = true; break;
        }
    }

    // Build GameResult
    GameResult res{};
    res.rounds = (size_t)step;

    int alive1 = 0, alive2 = 0;
    for (auto& t : tanks) if (t.alive) { if (t.player==1) ++alive1; else ++alive2; }

    if (alive1 == 0 && alive2 == 0) {
        res.winner = 0;
        res.reason = GameResult::ALL_TANKS_DEAD;
        res.remaining_tanks = {0,0};
    } else if (alive1 == 0) {
        res.winner = 2;
        res.reason = GameResult::ALL_TANKS_DEAD;
        res.remaining_tanks = {0, (size_t)alive2};
    } else if (alive2 == 0) {
        res.winner = 1;
        res.reason = GameResult::ALL_TANKS_DEAD;
        res.remaining_tanks = {(size_t)alive1, 0};
    } else if (rounds_without_shells >= rounds_without_shells_limit) {
        res.winner = 0;
        res.reason = GameResult::ZERO_SHELLS;
        // counts unknown from rule; we can still report alive counts:
        res.remaining_tanks = {(size_t)alive1, (size_t)alive2};
    } else {
        // max steps reached
        res.winner = 0;
        res.reason = GameResult::MAX_STEPS;
        res.remaining_tanks = {(size_t)alive1, (size_t)alive2};
    }

    // create final snapshot grid from current board + alive tanks
    std::vector<std::string> finalGrid = board; // walls/mines already inside
    for (const auto& t : tanks) if (t.alive) {
        finalGrid[t.y][t.x] = (t.player==1 ? '1' : '2');
    }
    res.gameState = std::make_unique<SnapshotSatelliteView>(std::move(finalGrid));
    if (verbose_ && fout) {
        if (res.winner == 1) {
            fout << "Player 1 won with " << res.remaining_tanks[0] << " tanks still alive\n";
        } else if (res.winner == 2) {
            fout << "Player 2 won with " << res.remaining_tanks[1] << " tanks still alive\n";
        } else {
            if (res.reason == GameResult::MAX_STEPS) {
                fout << "Tie, reached max steps = " << maxSteps
                     << ", player 1 has " << res.remaining_tanks[0]
                     << " tanks, player 2 has " << res.remaining_tanks[1] << " tanks\n";
            } else if (res.reason == GameResult::ALL_TANKS_DEAD) {
                fout << "Tie, both players have zero tanks\n";
            } else { // ZERO_SHELLS
                fout << "Tie, zero shells\n";
            }
        }
        fout << res.rounds << "\n";
        for (const auto& row : finalGrid) fout << row << "\n";
    }


    return res;
}

//using GameManagerType = GameManager_212934582_323964676::GameManager_212934582_323964676;
//REGISTER_GAME_MANAGER(GameManagerType)

// GameManager/GameManager_322837089_212141253.cpp

namespace GameManager_322837089_212141253 {
    // … class GameManager definition …
    // Registration:
    REGISTER_GAME_MANAGER(GameManager);
} // namespace


