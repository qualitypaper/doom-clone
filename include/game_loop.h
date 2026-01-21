#pragma once

#include "entity.h"
#include "sdl_window.h"
#include "config.h"
#include <vector>

/* 
 X, Y - horizontal planes, X - east-west, Y - north-south
 Z - vertical plane, 
*/

namespace gameloop
{
    void handleMouseMovement(SDL_Event &event, InputState &input);
    void handleKeyInput(SDL_Event &event, InputState &input);

    struct WorldState
    {
        // BSP nodes, sectors,
        int placeholder;
    };

    struct GameState
    {
        entity::Player playerState;
        WorldState worldState;
        std::vector<entity::EntityState> entityState;
        uint32_t rng_seed;
        time_t gameTime;
    };

    struct SideDef
    {
        int16_t xOffset;
        int16_t yOffset;
        int16_t sectorId;
    };

    enum class LineDefType
    {
        REGULAR
    };

    struct LineDef
    {
        int16_t start;
        int16_t end;
        LineDefType type;
        int16_t frontSidedef;
        int16_t backSidedef;
    };

    struct Sector
    {
        int16_t floorHeight;
        int16_t ceilingHeight;
        int16_t specialType;
        int16_t lightLevel;
        int16_t tag;
    };
}