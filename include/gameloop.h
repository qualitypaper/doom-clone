#pragma once

#include "entity.h"
#include "sdl_window.h"
#include "serialization.h"

#include <array>
#include <vector>

/*
 X, Y - horizontal planes, X - east-west, Y - north-south
 Z - vertical plane,
*/

// forward declarations
struct Seg;
struct SubSector;
struct BspLevel;
struct BspNode;


enum class EngineMode { GAMEPLAY_3D, EDITOR_2D, BSP_VIEWER };

struct GameState
{
  entity::Player playerState;
  std::vector<entity::EntityState> entityState;
  uint32_t rng_seed;
  time_t gameTime;
  EngineMode currentMode;
  uint16_t levelNum;
};

struct Vertex : public Serializable
{
  int32_t x = 0;
  int32_t y = 0;

  Vertex() = default;
  Vertex(int32_t _x, int32_t _y) : x(_x), y(_y) {}

  Vertex operator+(const Vertex &other) const { return Vertex{ x + other.x, y + other.y }; }
  Vertex operator-(const Vertex &other) const { return Vertex{ x - other.x, y - other.y }; }
  // dot product
  int32_t operator*(const Vertex &other) const { return x * other.x + y * other.y; }
  Vertex operator*(const double c) const { return Vertex{ static_cast<int32_t>(c * x), static_cast<int32_t>(c * y) }; }
  Vertex operator/(const double c) const { return Vertex{ static_cast<int32_t>(x / c), static_cast<int32_t>(y / c) }; }

  bool operator==(const Vertex &other) const { return x == other.x && y == other.y; }

  void serialize(FileWriter &fw) const override;
  void deserialize(FileReader &fr) override;
};

struct SideDef : public Serializable
{
  int16_t sectorId = -1;
  int16_t xOffset = 0;
  int16_t yOffset = 0;
  int16_t upperWallTexture = -1;
  int16_t middleWallTexture = -1;
  int16_t bottomWallTexture = -1;

  SideDef() = default;

  SideDef(int16_t _sectorId,
    int16_t _xOffset,
    int16_t _yOffset,
    int16_t _upperWallTexture,
    int16_t _middleWallTexture,
    int16_t _bottomWallTexture)
    : sectorId(_sectorId), xOffset(_xOffset), yOffset(_yOffset), upperWallTexture(_upperWallTexture),
      middleWallTexture(_middleWallTexture), bottomWallTexture(_bottomWallTexture)
  {}

  void serialize(FileWriter &fw) const override;
  void deserialize(FileReader &fr) override;
};

enum class LineDefType { REGULAR, DOOR };

struct LineDef : public Serializable
{
  int16_t start = -1;
  int16_t end = -1;
  LineDefType type = LineDefType::REGULAR;
  int16_t frontSidedef = -1;
  int16_t backSidedef = -1;

  LineDef() = default;
  LineDef(int16_t _start, int16_t _end, LineDefType _type, int16_t _frontSidedef, int16_t _backSidedef)
    : start(_start), end(_end), type(_type), frontSidedef(_frontSidedef), backSidedef(_backSidedef)
  {}

  void serialize(FileWriter &fw) const override;
  void deserialize(FileReader &fr) override;
};

struct Sector : public Serializable
{
  int16_t floorHeight = 0;
  int16_t ceilingHeight = 0;
  int16_t floorTextureIndex = -1;
  int16_t ceilingTextureIndex = -1;
  int16_t lightLevel = 0;
  int16_t specialType = 0;
  int16_t tag = 0;

  Sector() = default;

  Sector(int16_t _floorHeight,
         int16_t _ceilingHeight,
         int16_t _floorTextureIndex,
         int16_t _ceilingTextureIndex,
         int16_t _lightLevel,
         int16_t _specialType,
         int16_t _tag,
         uint32_t _color = 0xFFFFFFFF)
    : floorHeight(_floorHeight),
      ceilingHeight(_ceilingHeight),
      floorTextureIndex(_floorTextureIndex),
      ceilingTextureIndex(_ceilingTextureIndex),
      lightLevel(_lightLevel),
      specialType(_specialType),
      tag(_tag),
      color(_color)
  {}

  // temporary, till textures will not be added, RGBA format
  // default - BLACK
  uint32_t color = 0xFFFFFFFF;

  void serialize(FileWriter &fw) const override;
  void deserialize(FileReader &fr) override;
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

  void InitializeBspParams(std::unique_ptr<BspLevel> bspLevel);
  void Load(FileReader &fr);
};
void HandleMouseMovement(const SDL_Event &event, InputState &input);
void HandleKeyInput(const SDL_Event &event, InputState &input);
void SetEngineMode(GameState &gameState, InputState &input, EngineMode newMode, const SdlWindow &sdlWindow);
