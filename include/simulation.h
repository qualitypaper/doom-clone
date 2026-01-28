#pragma once

#include "gameloop.h"

namespace simulation {

    void update(gameloop::GameState &gameState, InputState &input, const double_t dt);
}