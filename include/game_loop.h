#pragma once

#include "entity.h"
#include <vector>

namespace game_loop
{
    struct WorldState
    {
        // BSP nodes, sectors,
    };
    
    struct GameState
    {
        entity::Player playerState;
        WorldState worldState;
        std::vector<entity::EntityState> entityState;
        uint32_t rng_seed;
        time_t gameTime;
    };

}