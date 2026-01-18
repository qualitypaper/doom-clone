#include "config.h"
#include "camera.h"

// coords begin at the left bottom corner
namespace camera
{

    glm::vec2 project(glm::vec2 vec, float_t z)
    {
        return glm::vec2{
            X_PROJECTION * vec.x / z,
            Y_PROJECTION * vec.y / z};
    }

    glm::vec2 unproject(glm::vec2 point, float_t z)
    {
        return glm::vec2{
            point.x * z / X_PROJECTION,
            point.y * z / Y_PROJECTION
        };
    }
}
