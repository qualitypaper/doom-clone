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

struct Vertex
{
  int32_t x = 0;
  int32_t y = 0;

  Vertex() = default;
  Vertex(const int32_t _x, const int32_t _y) : x(_x), y(_y) {}

  Vertex operator+(const Vertex &other) const { return Vertex{ x + other.x, y + other.y }; }
  Vertex operator-(const Vertex &other) const { return Vertex{ x - other.x, y - other.y }; }
  // dot product
  int32_t operator*(const Vertex &other) const { return x * other.x + y * other.y; }
  Vertex operator*(const double c) const { return Vertex{ static_cast<int32_t>(c * x), static_cast<int32_t>(c * y) }; }
  Vertex operator/(const double c) const { return Vertex{ static_cast<int32_t>(x / c), static_cast<int32_t>(y / c) }; }

  bool operator==(const Vertex &other) const { return x == other.x && y == other.y; }

  template<typename Writer>
  void serialize(Writer &w) const
    requires serialization::HasWriteRaw<Writer, Vertex>
  {
    serialization::serialize(w, x);
    serialization::serialize(w, y);
  }
  template<typename Reader> void deserialize(Reader &fr)
  {
    serialization::deserialize<Reader, decltype(x)>(fr, x);
    serialization::deserialize(fr, y);
  }
};

struct SideDef
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

  template<typename Writer> void serialize(Writer &w) const
  {
    serialization::serialize(w, sectorId);
    serialization::serialize(w, upperWallTexture);
    serialization::serialize(w, middleWallTexture);
    serialization::serialize(w, bottomWallTexture);
    serialization::serialize(w, xOffset);
    serialization::serialize(w, yOffset);
  }

  template<typename Reader> void deserialize(Reader &r)
  {
    serialization::deserialize(r, sectorId);
    serialization::deserialize(r, upperWallTexture);
    serialization::deserialize(r, middleWallTexture);
    serialization::deserialize(r, bottomWallTexture);
    serialization::deserialize(r, xOffset);
    serialization::deserialize(r, yOffset);
  }
};

enum class LineDefType { REGULAR, DOOR };

struct LineDef
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

  template<typename Writer> void serialize(Writer &w) const
  {
    serialization::serialize(w, start);
    serialization::serialize(w, end);

    const int8_t intType = static_cast<int8_t>(type);

    serialization::serialize(w, intType);

    serialization::serialize(w, frontSidedef);
    serialization::serialize(w, backSidedef);
  }

  template<typename Reader> void deserialize(Reader &r)
  {
    serialization::deserialize(r, start);
    serialization::deserialize(r, end);

    int8_t intType;
    serialization::deserialize(r, intType);
    type = static_cast<LineDefType>(intType);

    serialization::deserialize(r, frontSidedef);
    serialization::deserialize(r, backSidedef);
  }
};

struct Sector
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
    : floorHeight(_floorHeight), ceilingHeight(_ceilingHeight), floorTextureIndex(_floorTextureIndex),
      ceilingTextureIndex(_ceilingTextureIndex), lightLevel(_lightLevel), specialType(_specialType), tag(_tag),
      color(_color)
  {}

  // temporary, till textures will not be added, RGBA format
  // default - BLACK
  uint32_t color = 0xFFFFFFFF;

  template<typename Writer> void serialize(Writer &w) const
  {
    serialization::serialize(w, floorHeight);
    serialization::serialize(w, ceilingHeight);
    serialization::serialize(w, floorTextureIndex);
    serialization::serialize(w, ceilingTextureIndex);
    serialization::serialize(w, lightLevel);
    serialization::serialize(w, specialType);
    serialization::serialize(w, tag);
    serialization::serialize(w, color);
  }

  template<typename Reader> void deserialize(Reader &r)
  {
    serialization::deserialize(r, floorHeight);
    serialization::deserialize(r, ceilingHeight);
    serialization::deserialize(r, floorTextureIndex);
    serialization::deserialize(r, ceilingTextureIndex);
    serialization::deserialize(r, lightLevel);
    serialization::deserialize(r, specialType);
    serialization::deserialize(r, tag);
    serialization::deserialize(r, color);
  }
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
  std::array<char8_t, 8> name{};

  void InitializeBspParams(std::unique_ptr<BspLevel> bspLevel);
  void Load(FileReader &fr);
};
void HandleMouseMovement(const SDL_Event &event, InputState &input);
void HandleKeyInput(const SDL_Event &event, InputState &input);
void SetEngineMode(GameState &gameState, InputState &input, EngineMode newMode, const SdlWindow &sdlWindow);

inline std::array<char8_t, 8> MakeLevelName(uint16_t number)
{
  const std::string s = std::format("Map{}", number);
  std::array<char8_t, 8> arr{};

  const size_t len = std::min(s.size(), size_t{8});
  for (size_t i = 0; i < len; ++i) {
    arr[i] = static_cast<char8_t>(s[i]);
  }
  return arr;
}
