//
// Created by qualitypaper on 3/31/26.
//

#ifndef DOOMCLONE_RENDERER_HELPER_H
#define DOOMCLONE_RENDERER_HELPER_H
#include "core/fixed_math.h"
#include "tables.h"


#include <cstdint>

// forward declaration
struct Player;

uint32_t SlopeDiv(uint32_t num, uint32_t den);
angle_t PointToAngle(fixed_t x, fixed_t y, fixed_t viewx, fixed_t viewy);
angle_t PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
fixed_t PointToDist(fixed_t x, fixed_t y, const Player &player);

#endif //DOOMCLONE_RENDERER_HELPER_H
