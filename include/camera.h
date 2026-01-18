#pragma once

#include "config.h"
#include <glm/glm.hpp>

namespace camera
{

    constexpr uint16_t X_PROJECTION = config::PROJECTION_PLANE_DISTANCE * config::CANVAS_WIDTH / config::VIEWPORT_WIDTH;
    constexpr uint16_t Y_PROJECTION = config::PROJECTION_PLANE_DISTANCE * config::CANVAS_HEIGHT / config::VIEWPORT_HEIGHT;

    constexpr uint16_t X_UNPROJECTION = 1 / X_PROJECTION;
    constexpr uint16_t Y_UNPROJECTION = 1 / Y_PROJECTION;

    glm::vec2 project(glm::vec2 vec, float_t z);
    glm::vec2 unproject(glm::vec2 point, float_t z);
}