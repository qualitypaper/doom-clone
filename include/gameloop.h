#pragma once

#include "entity.h"
#include "sdl_window.h"
#include <vector>

/*
 X, Y - horizontal planes, X - east-west, Y - north-south
 Z - vertical plane,
*/

enum class EngineMode { GAMEPLAY_3D, EDITOR_2D };

struct WorldState
{
  // BSP nodes, sectors,
  int placeholder;
};

struct GameState
{
  entity::Player playerState;
  WorldState worldState;
  std::vector<entity::EntityState> entityState;
  uint32_t rng_seed;
  time_t gameTime;
  EngineMode currentMode;
};

struct Vertex
{
  int32_t x = 0;
  int32_t y = 0;

  Vertex operator+(const Vertex &other) const { return Vertex{ x + other.x, y + other.y }; }
  Vertex operator-(const Vertex &other) const { return Vertex{ x - other.x, y - other.y }; }
};

struct SideDef
{
  int16_t sectorId = -1;
  int16_t xOffset = 0;
  int16_t yOffset = 0;
  uint32_t color = 0xFFFFFFFF;// RGBA format
};

enum class LineDefType { REGULAR, DOOR };

struct LineDef
{
  int16_t start;
  int16_t end;
  LineDefType type;
  int16_t frontSidedef;
  int16_t backSidedef;
};

struct Sector
{
  int16_t floorHeight;
  int16_t ceilingHeight;
  int16_t specialType;
  int16_t lightLevel;
  int16_t tag;
};

struct Level
{
  std::vector<Vertex> vertices;
  std::vector<LineDef> linedefs;
  std::vector<SideDef> sidedefs;
  std::vector<Sector> sectors;

  void serialize(const char *filename);
  bool deserialize(const char *filename);
};

void handleMouseMovement(const SDL_Event &event, InputState &input);
void handleKeyInput(SDL_Event &event, InputState &input);
void setEngineMode(GameState &gameState, InputState &input, EngineMode newMode, SdlWindow &sdlWindow);
