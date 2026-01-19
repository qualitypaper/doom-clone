#pragma once

#include "config.h"
#include <glm/glm.hpp>

namespace camera
{

    constexpr uint16_t X_PROJECTION = config::PROJECTION_PLANE_DISTANCE * config::CANVAS_WIDTH / config::VIEWPORT_WIDTH;
    constexpr uint16_t Z_PROJECTION = config::PROJECTION_PLANE_DISTANCE * config::CANVAS_HEIGHT / config::VIEWPORT_HEIGHT;

    glm::vec2 project(int16_t x, int16_t y, float_t depth);
    glm::vec2 unproject(glm::vec2 point, float_t z);
}