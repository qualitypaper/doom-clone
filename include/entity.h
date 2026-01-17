#pragma once
#include <cstdint>
#include "glm/glm.hpp"

namespace entity
{

    struct Player
    {
        glm::vec2 position;
        float_t z;
        float_t velocity;
        float angle;
        uint8_t health{100};
        uint8_t armor;
        uint8_t current_weapon;
    };

    enum class EntityType
    {
        Imp,
        Projectile,
        Pickup
    };

    struct EntityState
    {
        EntityType type;
        glm::vec2 position;
        float z;
        glm::vec2 velocity;
        int health;
    };

}
