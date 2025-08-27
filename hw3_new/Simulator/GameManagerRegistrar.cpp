#include "GameManagerRegistrar.h"

GameManagerRegistrar GameManagerRegistrar::singleton_;

GameManagerRegistrar& GameManagerRegistrar::get() {
    return singleton_;
}
