#pragma once

#include "gameloop.h"

namespace simulation {

    void update(gameloop::GameState &gameState, const InputState &input, double_t dt);
}