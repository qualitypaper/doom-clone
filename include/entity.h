#pragma once
#include <cstdint>
#include "glm/glm.hpp"

namespace entity
{

    struct Player
    {
        double_t x;
        double_t y;
        double_t z;
        double_t velocity;
        double_t angle;
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
