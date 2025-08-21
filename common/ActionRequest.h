#ifndef COMMON_ACTIONREQUEST_H
#define COMMON_ACTIONREQUEST_H

enum class ActionRequest { 
    MoveForward, MoveBackward,
    RotateLeft90, RotateRight90, RotateLeft45, RotateRight45,
    Shoot, GetBattleInfo, DoNothing
};

#endif // COMMON_ACTIONREQUEST_H
