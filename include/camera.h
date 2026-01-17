#pragma once

#include "config.h"
#include <glm/glm.hpp>

namespace camera
{

    glm::vec2 project(glm::vec2 vec, float_t z);
    glm::vec3 unproject(glm::vec2 point);
}