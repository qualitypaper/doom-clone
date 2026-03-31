#pragma once

#include "../../include/defs.h"
#include "sdl_window.h"

#include <array>
#include <cmath>
#include <vector>

/*
 X, Y - horizontal planes, X - east-west, Y - north-south
 Z - vertical plane,
*/
extern std::array<fixed_t, FINE_ANGLES/2> viewangletox;
extern std::array<fixed_t, config::CANVAS_WIDTH + 1> xtoviewangle;

enum class EngineMode { GAMEPLAY_3D, EDITOR_2D, BSP_VIEWER };

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
  Vertex position;
  float z;
  Vertex velocity;
  int health;
};

struct GameState
{
  Player playerState{};
  std::vector<EntityState> entityState{};
  uint32_t rng_seed{};
  time_t gameTime{};
  EngineMode currentMode;
  uint16_t levelNum{};
};

struct Level
{
  std::vector<Vertex> vertices;
  std::vector<line_t> linedefs;
  std::vector<side_t> sidedefs;
  std::vector<Sector> sectors;
  std::vector<BspNode> nodes;
  std::vector<SubSector> subsectors;
  std::vector<seg_t> segments;
  std::array<char8_t, 8> name{};

  void Load(FileReader &fr);

  static void Init(Level &level,
    const std::vector<LineDef> &_lines,
    const std::vector<SideDef> &_sides,
    const std::vector<Seg> &_segs);
};

void HandleMouseMovement(const SDL_Event &event, InputState &input);
void HandleKeyInput(const SDL_Event &event, InputState &input);
void SetEngineMode(GameState &gameState, InputState &input, EngineMode newMode, const SdlWindow &sdlWindow);

inline std::array<char8_t, 8> MakeLevelName(uint16_t number)
{
  const std::string s = std::format("Map{}", number);
  std::array<char8_t, 8> arr{};

  const size_t len = std::min(s.size(), size_t{ 8 });
  for (size_t i = 0; i < len; ++i) {
    arr[i] = static_cast<char8_t>(s[i]);
  }
  return arr;
}
