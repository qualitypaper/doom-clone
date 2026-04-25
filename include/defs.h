#ifndef DOOMCLONE_DEFS_H
#define DOOMCLONE_DEFS_H
#include "config.h"
#include "core/fixed_math.h"
#include "core/serialization/serialization.h"
#include "imgui/imgui.h"
#include "tables.h"

#include <array>
#include <cstdint>
#include <vector>

using lumpName = std::array<char8_t, 8>;

struct Visplane
{
  fixed_t height = 0;
  int16_t textureIndex = -1;
  int lightLevel = 0;
  int minX = 0;
  int maxX = 0;

  std::vector<int16_t> top;
  std::vector<int16_t> bottom;

  Visplane() = default;
  Visplane(const fixed_t _height,
           const int16_t _texIndex,
           const int _lightLevel,
           const int _minX,
           const int _maxX,
           const std::size_t width)
    : height(_height), textureIndex(_texIndex), lightLevel(_lightLevel), minX(_minX), maxX(_maxX), top(width, -1),
      bottom(width, -1)
  {}
};

struct ClipRange
{
  int start = 0;
  int end = 0;

  ClipRange() = default;
  ClipRange(const int _start, const int _end) : start(_start), end(_end)
  {}
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

  Sector(const fixed_t _floorHeight,
         const fixed_t _ceilingHeight,
         const int16_t _floorTextureIndex,
         const int16_t _ceilingTextureIndex,
         const int16_t _lightLevel,
         const int16_t _specialType,
         const int16_t _tag)
    : floorHeight(_floorHeight), ceilingHeight(_ceilingHeight), floorTextureIndex(_floorTextureIndex),
      ceilingTextureIndex(_ceilingTextureIndex), lightLevel(_lightLevel), specialType(_specialType), tag(_tag)
  {}

  template<typename Writer>
  void serialize(Writer &w) const
  {
    serialization::serialize(w, floorHeight);
    serialization::serialize(w, ceilingHeight);
    serialization::serialize(w, floorTextureIndex);
    serialization::serialize(w, ceilingTextureIndex);
    serialization::serialize(w, lightLevel);
    serialization::serialize(w, specialType);
    serialization::serialize(w, tag);
  }

  template<typename Reader>
  void deserialize(Reader &r)
  {
    serialization::deserialize(r, floorHeight);
    serialization::deserialize(r, ceilingHeight);
    serialization::deserialize(r, floorTextureIndex);
    serialization::deserialize(r, ceilingTextureIndex);
    serialization::deserialize(r, lightLevel);
    serialization::deserialize(r, specialType);
    serialization::deserialize(r, tag);
  }
};

struct Vertex
{
  fixed_t x = 0;
  fixed_t y = 0;

  Vertex() = default;
  Vertex(const int32_t _x, const int32_t _y) : x(_x), y(_y)
  {}

  Vertex operator+(const Vertex &other) const
  {
    return Vertex{ x + other.x, y + other.y };
  }
  Vertex operator-(const Vertex &other) const
  {
    return Vertex{ x - other.x, y - other.y };
  }
  // dot product
  fixed_t operator*(const Vertex &other) const
  {
    return FixedMul(x, other.x) + FixedMul(y, other.y);
  }
  Vertex operator*(const double c) const
  {
    return Vertex{ FixedMul(c, x), FixedMul(c, y) };
  }
  Vertex operator/(const double c) const
  {
    return Vertex{ FixedDiv(x, c), FixedDiv(y, c) };
  }
  bool operator==(const Vertex &other) const
  {
    return x == other.x && y == other.y;
  }

  // gives the length of cross product v \cross other
  [[nodiscard]] fixed_t cross(const Vertex &other) const
  {
    return FixedMul(x, other.y) - FixedMul(y, other.x);
  }

  [[nodiscard]] Vertex toCenterCoords(const uint16_t width, const uint16_t height) const
  {
    return { x - ((width / 2) << FRAC_BITS), ((height / 2) << FRAC_BITS) - y };
  }

  [[nodiscard]] Vertex fromCenterCoords(const uint16_t width, const uint16_t height) const
  {
    return { std::clamp(((width / 2) << FRAC_BITS) + x, 0, width << FRAC_BITS),
             std::clamp(((height / 2) << FRAC_BITS) - y, 0, height << FRAC_BITS) };
  }

  [[nodiscard]] ImVec2 toImVec() const
  {
    return { static_cast<float>(FixedToDouble(x)), static_cast<float>(FixedToDouble(y)) };
  }


  template<typename Writer>
  void serialize(Writer &w) const
    requires serialization::HasWriteRaw<Writer, Vertex>
  {
    serialization::serialize(w, x);
    serialization::serialize(w, y);
  }
  template<typename Reader>
  void deserialize(Reader &fr)
  {
    serialization::deserialize(fr, x);
    serialization::deserialize(fr, y);
  }
};

struct side_t
{
  Sector *sector{};

  fixed_t xOffset{};
  fixed_t yOffset{};

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

  SideDef(const int16_t _sectorId,
          const fixed_t _xOffset,
          const fixed_t _yOffset,
          const int16_t _upperWallTexture,
          const int16_t _middleWallTexture,
          const int16_t _bottomWallTexture)
    : sectorId(_sectorId), xOffset(_xOffset), yOffset(_yOffset), upperWallTexture(_upperWallTexture),
      middleWallTexture(_middleWallTexture), bottomWallTexture(_bottomWallTexture)
  {}

  template<typename Writer>
  void serialize(Writer &w) const
  {
    serialization::serialize(w, sectorId);
    serialization::serialize(w, upperWallTexture);
    serialization::serialize(w, middleWallTexture);
    serialization::serialize(w, bottomWallTexture);
    serialization::serialize(w, xOffset);
    serialization::serialize(w, yOffset);
  }

  template<typename Reader>
  void deserialize(Reader &r)
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

  template<typename Writer>
  void serialize(Writer &w) const
  {
    serialization::serialize(w, start);
    serialization::serialize(w, end);

    const int8_t intType = static_cast<int8_t>(type);

    serialization::serialize(w, intType);

    serialization::serialize(w, frontSidedef);
    serialization::serialize(w, backSidedef);
  }

  template<typename Reader>
  void deserialize(Reader &r)
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

  template<typename Writer>
  void serialize(Writer &w) const
  {
    w.WriteRaw(startVertex);
    w.WriteRaw(endVertex);
    w.WriteRaw(angle);
    w.WriteRaw(linedefIndex);
    w.WriteRaw(side);
    w.WriteRaw(offset);
  }

  template<typename Reader>
  void deserialize(Reader &r)
  {
    r.ReadRaw(startVertex);
    r.ReadRaw(endVertex);
    r.ReadRaw(angle);
    r.ReadRaw(linedefIndex);
    r.ReadRaw(side);
    r.ReadRaw(offset);
  }
};

struct Node
{
  Node() = default;
  Node(fixed_t _x, fixed_t _y, fixed_t _dx, fixed_t _dy);

  fixed_t x = 0, y = 0;
  fixed_t dx = 0, dy = 0;

  std::array<fixed_t, 4> leftBoundingBox{ INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX };
  std::array<fixed_t, 4> rightBoundingBox{ INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX };

  int16_t leftChild = -1, rightChild = -1;

  template<typename Writer>
  void serialize(Writer &w) const
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

  template<typename Reader>
  void deserialize(Reader &r)
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
  int16_t segCount{};
  int16_t firstSegIndex{};

  SubSector() = default;
  SubSector(const int16_t _segCount, const int16_t _firstSegIndex) : segCount(_segCount), firstSegIndex(_firstSegIndex)
  {}

  template<typename Writer>
  void serialize(Writer &w) const
  {
    w.WriteRaw(segCount);
    w.WriteRaw(firstSegIndex);
  }
  template<typename Reader>
  void deserialize(Reader &r)
  {
    r.ReadRaw(segCount);
    r.ReadRaw(firstSegIndex);
  }
};


#endif// DOOMCLONE_DEFS_H
