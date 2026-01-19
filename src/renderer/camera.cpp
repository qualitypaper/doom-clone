#include "config.h"
#include "camera.h"

namespace camera
{

    glm::vec2 project(int16_t x, int16_t y, float_t depth)
    {
        return glm::vec2{
            x / depth,
            y / depth};
    }

    glm::vec2 unproject(glm::vec2 point, float_t z)
    {
        return glm::vec2{
            point.x * z / X_PROJECTION,
            point.y * z / Z_PROJECTION
        };
    }
}
