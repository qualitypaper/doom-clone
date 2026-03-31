#ifndef DOOMCLONE_DEFS_H
#define DOOMCLONE_DEFS_H
#include "config.h"
#include "core/fixed_math.h"
#include "core/serialization.h"
#include "tables.h"

#include <array>
#include <cstdint>


struct Visplane
{
  fixed_t height = 0;
  // TODO: change to texture index
  uint32_t color = -1;
  int lightLevel = 0;
  int minX = 0;
  int maxX = 0;

  int8_t top[config::CANVAS_WIDTH]{};
  int8_t bottom[config::CANVAS_WIDTH]{};
};

struct ClipRange
{
  int start = 0;
  int end = 0;

  ClipRange() = default;
  ClipRange(int16_t _start, int16_t _end) : start(_start), end(_end) {}
};

struct Sector
{
  fixed_t floorHeight = 0;
  fixed_t ceilingHeight = 0;
  int16_t floorTextureIndex = -1;
  int16_t ceilingTextureIndex = -1;
  int16_t lightLevel = 0;
  int16_t specialType = 0;
  int16_t tag = 0;

  Sector() = default;

  Sector(fixed_t _floorHeight,
    fixed_t _ceilingHeight,
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

struct Vertex
{
  fixed_t x = 0;
  fixed_t y = 0;

  Vertex() = default;
  Vertex(const int32_t _x, const int32_t _y) : x(_x), y(_y) {}

  Vertex operator+(const Vertex &other) const { return Vertex{ x + other.x, y + other.y }; }
  Vertex operator-(const Vertex &other) const { return Vertex{ x - other.x, y - other.y }; }
  // dot product
  fixed_t operator*(const Vertex &other) const { return FixedMul(x, other.x) + FixedMul(y, other.y); }
  Vertex operator*(const double c) const { return Vertex{ FixedMul(c, x), FixedMul(c, y) }; }
  Vertex operator/(const double c) const { return Vertex{ FixedDiv(x, c), FixedDiv(y, c) }; }

  // gives the length of cross product v \cross other
  fixed_t cross(const Vertex &other) const { return FixedMul(x, other.y) - FixedMul(y, other.x); }

  Vertex toCenterCoords(const uint16_t width, const uint16_t height) const
  {
    return { x - ((width / 2) << FRAC_BITS), ((height / 2) << FRAC_BITS) - y };
  }

  Vertex fromCenterCoords(const uint16_t width, const uint16_t height) const
  {
    return { std::clamp(((width / 2) << FRAC_BITS) + x, 0, width << FRAC_BITS),
      std::clamp(((height / 2) << FRAC_BITS) - y, 0, height << FRAC_BITS) };
  }

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
    serialization::deserialize(fr, x);
    serialization::deserialize(fr, y);
  }
};

struct side_t
{
  Sector *sector;

  fixed_t xOffset;
  fixed_t yOffset;

  int16_t upperWallTexture = -1;
  int16_t middlWallTexture = -1;
  int16_t bottomWallTexture = -1;

  side_t() = default;
  side_t(Sector *_sector,
    const fixed_t _xOffset,
    const fixed_t _yOffset,
    const int16_t _upper,
    const int16_t _middle,
    const int16_t _bottom)
    : sector(_sector), xOffset(_xOffset), yOffset(_yOffset), upperWallTexture(_upper), middlWallTexture(_middle),
      bottomWallTexture(_bottom)
  {}
};

struct SideDef
{
  int16_t sectorId = -1;
  fixed_t xOffset = 0;
  fixed_t yOffset = 0;
  int16_t upperWallTexture = -1;
  int16_t middleWallTexture = -1;
  int16_t bottomWallTexture = -1;

  SideDef() = default;

  SideDef(int16_t _sectorId,
    fixed_t _xOffset,
    fixed_t _yOffset,
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

struct line_t
{
  Vertex *start{};
  Vertex *end{};

  LineDefType type = LineDefType::REGULAR;

  side_t *frontSide{};
  side_t *backSide{};

  line_t() = default;
  line_t(Vertex *_start, Vertex *_end, LineDefType _type, side_t *_front, side_t *_back)
    : start(_start), end(_end), type(_type), frontSide(_front), backSide(_back)
  {}
  line_t(int16_t _start, int16_t _end, LineDefType _type, int16_t _front, int16_t _back)
    : start(nullptr), end(nullptr), type(_type), frontSide(nullptr), backSide(nullptr)
  {}
};


struct LineDef
{
  int32_t start = -1;
  int32_t end = -1;
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
struct seg_t
{
  Vertex *start{}, *end{};
  angle_t angle{};

  line_t *line{};

  int16_t side = -1;

  fixed_t offset = 0;
};


struct Seg
{
  int16_t startVertex = -1;
  int16_t endVertex = -1;
  angle_t angle = 0;
  int16_t linedefIndex = -1;
  int16_t side = -1;// 0 for front, 1 for back
  fixed_t offset = 0;

  Seg() = default;
  Seg(int16_t _startVertex, int16_t _endVertex, angle_t _angle, int16_t _linedefIndex, int16_t _side, fixed_t _offset)
    : startVertex(_startVertex), endVertex(_endVertex), angle(_angle), linedefIndex(_linedefIndex), side(_side),
      offset(_offset)

  {}

  bool operator==(const Seg &other) const
  {
    return startVertex == other.startVertex && endVertex == other.endVertex && linedefIndex == other.linedefIndex;
  }

  template<typename Writer> void serialize(Writer &w) const
  {
    w.WriteRaw(startVertex);
    w.WriteRaw(endVertex);
    w.WriteRaw(angle);
    w.WriteRaw(linedefIndex);
    w.WriteRaw(side);
    w.WriteRaw(offset);
  }

  template<typename Reader> void deserialize(Reader &r)
  {
    r.ReadRaw(startVertex);
    r.ReadRaw(endVertex);
    r.ReadRaw(angle);
    r.ReadRaw(linedefIndex);
    r.ReadRaw(side);
    r.ReadRaw(offset);
  }
};

struct BspNode
{
  BspNode() = default;
  BspNode(fixed_t _x, fixed_t _y, fixed_t _dx, fixed_t _dy);

  fixed_t x = 0, y = 0;
  fixed_t dx = 0, dy = 0;

  std::array<fixed_t, 4> leftBoundingBox{ INT32_MAX, INT32_MIN, INT32_MIN, INT32_MAX };
  std::array<fixed_t, 4> rightBoundingBox{ INT32_MAX, INT32_MIN, INT32_MIN, INT32_MAX };

  int16_t leftChild = -1, rightChild = -1;

  template<typename Writer> void serialize(Writer &w) const
  {
    w.WriteRaw(x);
    w.WriteRaw(y);
    w.WriteRaw(dx);
    w.WriteRaw(dy);

    w.WriteRaw(leftBoundingBox);
    w.WriteRaw(rightBoundingBox);

    w.WriteRaw(leftChild);
    w.WriteRaw(rightChild);
  }

  template<typename Reader> void deserialize(Reader &r)
  {
    r.ReadRaw(x);
    r.ReadRaw(y);
    r.ReadRaw(dx);
    r.ReadRaw(dy);

    r.ReadRaw(leftBoundingBox);
    r.ReadRaw(rightBoundingBox);

    r.ReadRaw(leftChild);
    r.ReadRaw(rightChild);
  }
};


struct SubSector
{
  Sector *sector = nullptr;
  int16_t segCount;
  int16_t firstSegIndex;

  SubSector() = default;
  SubSector(const int16_t _segCount, const int16_t _firstSegIndex) : segCount(_segCount), firstSegIndex(_firstSegIndex)
  {}

  template<typename Writer> void serialize(Writer &w) const
  {
    w.WriteRaw(segCount);
    w.WriteRaw(firstSegIndex);
  }
  template<typename Reader> void deserialize(Reader &r)
  {
    r.ReadRaw(segCount);
    r.ReadRaw(firstSegIndex);
  }
};


#endif// DOOMCLONE_DEFS_H
