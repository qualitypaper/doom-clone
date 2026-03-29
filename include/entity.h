#pragma once
#include "glm/glm.hpp"
#include <cstdint>

struct Player
{
  int16_t x{};
  int16_t y{};
  int16_t z{};
  float_t velocity{};
  float_t angle{};
  uint8_t health{ 100 };
  uint8_t armor{};
  uint8_t current_weapon{};
};

enum class EntityType { Imp, Projectile, Pickup };

struct EntityState
{
  EntityType type;
  glm::vec2 position;
  float z;
  glm::vec2 velocity;
  int health;
};
