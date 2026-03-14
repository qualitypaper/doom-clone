#pragma once

#include "entity.h"
#include "sdl_window.h"

#include <array>
#include <vector>

/*
 X, Y - horizontal planes, X - east-west, Y - north-south
 Z - vertical plane,
*/


struct Seg;
struct SubSector;
struct BspNode;
enum class EngineMode { GAMEPLAY_3D, EDITOR_2D, BSP_VIEWER };

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
  // dot product
  int32_t operator*(const Vertex &other) const { return x * other.x + y * other.y; }
  Vertex operator*(const double c) const { return Vertex{ static_cast<int32_t>(c * x), static_cast<int32_t>(c * y) }; }
  Vertex operator/(const double c) const { return Vertex{ static_cast<int32_t>(x / c), static_cast<int32_t>(y / c) }; }

  bool operator==(const Vertex &other) const { return x == other.x && y == other.y; }
};

struct SideDef
{
  int16_t sectorId = -1;
  int16_t upperWallTexture = -1;
  int16_t middleWallTexture = -1;
  int16_t bottomWallTexture = -1;
  int16_t xOffset = 0;
  int16_t yOffset = 0;
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
  int16_t floorTextureIndex;
  int16_t ceilingTextureIndex;
  int16_t lightLevel;
  int16_t specialType;
  int16_t tag;


  // temporary, till textures will not be added, RGBA format
  // default - BLACK
  uint32_t color;
};

struct Level
{
  std::vector<Vertex> vertices;
  std::vector<LineDef> linedefs;
  std::vector<SideDef> sidedefs;
  std::vector<Sector> sectors;
  std::vector<BspNode> nodes;
  std::vector<SubSector> subsectors;
  std::vector<Seg> segments;

  void serialize(const char *filename);
  bool deserialize(const char *filename);
};

void HandleMouseMovement(const SDL_Event &event, InputState &input);
void HandleKeyInput(const SDL_Event &event, InputState &input);
void SetEngineMode(GameState &gameState, InputState &input, EngineMode newMode, const SdlWindow &sdlWindow);
