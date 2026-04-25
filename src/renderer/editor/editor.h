#pragma once

#include "bsp/bsp.h"
#include "core/gameloop.h"
#include "core/math_utils.h"
#include "core/serialization/serialization.h"
#include "defs.h"
#include "imgui/imgui.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <vector>


// forward declarations
class TextureManager;
class PaletteManager;

class BSPBuilder;
struct CommandHistory;
struct Command;

struct EditorState;


struct AABB
{
  AABB() = default;
  AABB(Vertex start, Vertex end);
  double maxX, maxY;
  double minX, minY;

  [[nodiscard]] bool contains(const double x, const double y) const
  {
    return x >= minX && x <= maxX && y >= minY && y <= maxY;
  }
};


enum class EditorObjectType { LINEDEF, VERTEX, SECTOR, SIDEDEF };

// Encoded object id: top 2 bits store type, remaining bits store index
constexpr uint32_t kObjectTypeShift = 30;
constexpr uint32_t kObjectIndexMask = (1u << kObjectTypeShift) - 1u;

constexpr uint32_t makeObjectId(EditorObjectType type, const uint32_t index)
{
  return (static_cast<uint32_t>(type) << kObjectTypeShift) | (index & kObjectIndexMask);
}

constexpr EditorObjectType getObjectType(const uint32_t objectId)
{
  return static_cast<EditorObjectType>((objectId >> kObjectTypeShift) & 0x3u);
}

constexpr uint32_t getObjectIndex(const uint32_t objectId)
{
  return objectId & kObjectIndexMask;
}

struct DraggableObject
{
  DraggableObject() = default;
  virtual ~DraggableObject() = default;

  bool dragged = false;

  virtual void drag(uint32_t objectId, EditorState *state, CommandHistory *history);
  static void resetDraggableState(EditorState *state);
};

struct EditorObject
  : public Serializable
  , public DraggableObject
{
  explicit EditorObject(const EditorObjectType _type) : type(_type)
  {}
  ~EditorObject() override = default;

  EditorObjectType type;
  bool selected = false;
  bool hovered = false;

  virtual void select()
  {
    this->selected = !this->selected;
  }

  virtual void hover()
  {
    this->hovered = !this->hovered;
  }
};

struct EditorLineDef : public EditorObject
{
  EditorLineDef() : EditorObject(EditorObjectType::LINEDEF)
  {}
  explicit EditorLineDef(const LineDef &_linedef)
    : EditorObject(EditorObjectType::LINEDEF), start(makeObjectId(EditorObjectType::VERTEX, _linedef.start)),
      end(makeObjectId(EditorObjectType::VERTEX, _linedef.end)), type(_linedef.type),
      frontSideDef(_linedef.frontSidedef), backSideDef(_linedef.backSidedef)
  {}
  EditorLineDef(const uint32_t _start,
                const uint32_t _end,
                const LineDefType _type,
                const int16_t _frontSideDef,
                const int16_t _backSideDef)
    : EditorObject(EditorObjectType::LINEDEF), start(_start), end(_end), type(_type), frontSideDef(_frontSideDef),
      backSideDef(_backSideDef)
  {}

  uint32_t start = 0;
  uint32_t end = 0;
  LineDefType type = LineDefType::REGULAR;
  int16_t frontSideDef = -1;
  int16_t backSideDef = -1;

  static void remove(const EditorState &state, uint32_t ldId);

  void serialize(FileWriter &fw) const override;
  void deserialize(FileReader &fr) override;

  void drag(uint32_t objectId, EditorState *state, CommandHistory *history) override;
};

struct EditorVertex : public EditorObject
{
  EditorVertex() : EditorObject(EditorObjectType::VERTEX)
  {}
  EditorVertex(const double _x, const double _y) : EditorObject(EditorObjectType::VERTEX), x(_x), y(_y)
  {}
  explicit EditorVertex(const ImVec2 &vec)
    : EditorObject(EditorObjectType::VERTEX), x(static_cast<int32_t>(vec.x)), y(static_cast<int32_t>(vec.y))
  {}
  explicit EditorVertex(const ImVec2 vec)
    : EditorObject(EditorObjectType::VERTEX), x(static_cast<int32_t>(vec.x)), y(static_cast<int32_t>(vec.y))
  {}

  explicit EditorVertex(const Vertex v) : EditorObject(EditorObjectType::VERTEX)
  {
    this->x = FixedToDouble(v.x);
    this->y = FixedToDouble(v.y);
  }

  double x = 0, y = 0;
  std::vector<uint32_t> connectedLineDefs = {};// Linedef object IDs (EditorObjectType::LINEDEF).

  EditorVertex operator+(const EditorVertex &other) const
  {
    return { x + other.x, y + other.y };
  }
  EditorVertex operator-(const EditorVertex &other) const
  {
    return { x - other.x, y - other.y };
  }

  EditorVertex operator+(const ImVec2 &other) const
  {
    return { x + other.x, y + other.y };
  }
  EditorVertex operator-(const ImVec2 &other) const
  {
    return { x - static_cast<int32_t>(other.x), y - static_cast<int32_t>(other.y) };
  }

  EditorVertex operator*(const double c) const
  {
    return { x * c, y * c };
  }
  EditorVertex operator/(const double c) const
  {
    return { x / c, y / c };
  }

  EditorVertex &operator+=(const ImVec2 &offset)
  {
    this->x += offset.x;
    this->y += offset.y;

    return *this;
  }
  EditorVertex &operator-=(const ImVec2 &offset)
  {
    this->x -= offset.x;
    this->y -= offset.y;

    return *this;
  }

  [[nodiscard]] bool isAnyConnectedLineDefSelected(const EditorState &state) const;
  [[nodiscard]] ImVec2 toImVec2() const
  {
    return { static_cast<float_t>(x), static_cast<float_t>(y) };
  }
  [[nodiscard]] double length() const
  {
    return std::sqrt(x * x + y * y);
  }

  void normalize()
  {
    const double len = length();

    x /= len;
    y /= len;
  }

  static void remove(EditorState &state, uint32_t vertexId);

  void serialize(FileWriter &fw) const override;
  void deserialize(FileReader &fr) override;

  void drag(uint32_t objectId, EditorState *state, CommandHistory *history) override;
};

struct EditorSidedef : EditorObject
{
  EditorSidedef() : EditorObject(EditorObjectType::SIDEDEF)
  {}
  EditorSidedef(const int16_t sectorId,
                const double xOffset,
                const double yOffset,
                const int16_t upperWallTexture,
                const int16_t middleWallTexture,
                const int16_t bottomWallTexture)
    : EditorObject(EditorObjectType::SIDEDEF), sectorId(sectorId), xOffset(xOffset), yOffset(yOffset),
      upperWallTexture(upperWallTexture), middleWallTexture(middleWallTexture), bottomWallTexture(bottomWallTexture)
  {}
  int16_t sectorId = -1;
  double xOffset = 0;
  double yOffset = 0;
  int16_t upperWallTexture = -1;
  int16_t middleWallTexture = -1;
  int16_t bottomWallTexture = -1;


  template<typename Writer>
  void serialize(Writer &w) const
  {
    serialization::serialize(w, sectorId);
    serialization::serialize(w, upperWallTexture);
    serialization::serialize(w, middleWallTexture);
    serialization::serialize(w, bottomWallTexture);
    serialization::serialize(w, DoubleToFixed(xOffset));
    serialization::serialize(w, DoubleToFixed(yOffset));
  }
  template<typename Reader>
  void deserialize(Reader &r)
  {
    serialization::deserialize(r, sectorId);
    serialization::deserialize(r, upperWallTexture);
    serialization::deserialize(r, middleWallTexture);
    serialization::deserialize(r, bottomWallTexture);

    fixed_t tempXOffset, tempYOffset;

    serialization::deserialize(r, tempXOffset);
    serialization::deserialize(r, tempYOffset);

    xOffset = FixedToDouble(tempXOffset);
    yOffset = FixedToDouble(tempYOffset);
  }
};

struct EditorSector : EditorObject
{
  EditorSector(const double floorHeight,
               const double ceilingHeight,
               const int16_t floorPic,
               const int16_t ceilingPic,
               const int16_t lightLevel,
               const int16_t specialType,
               const int16_t tag)
    : EditorObject(EditorObjectType::SECTOR), floorHeight(floorHeight), ceilingHeight(ceilingHeight),
      floorPic(floorPic), ceilingPic(ceilingPic), lightLevel(lightLevel), specialType(specialType), tag(tag)
  {}
  EditorSector() : EditorObject(EditorObjectType::SECTOR)
  {}

  double floorHeight = 0;
  double ceilingHeight = 0;
  int16_t floorPic = -1;
  int16_t ceilingPic = -1;
  int16_t lightLevel = 0;
  int16_t specialType = 0;
  int16_t tag = 0;
  AABB bounding_box{};
  std::vector<uint32_t> linedefIds;

  template<typename Writer>
  void serialize(Writer &w) const
  {
    serialization::serialize(w, DoubleToFixed(floorHeight));
    serialization::serialize(w, DoubleToFixed(ceilingHeight));
    serialization::serialize(w, floorPic);
    serialization::serialize(w, ceilingPic);
    serialization::serialize(w, lightLevel);
    serialization::serialize(w, specialType);
    serialization::serialize(w, tag);
  }

  template<typename Reader>
  void deserialize(Reader &r)
  {
    fixed_t floorHeightFixed, ceilHeightFixed;

    serialization::deserialize(r, floorHeightFixed);
    serialization::deserialize(r, ceilHeightFixed);

    floorHeight = FixedToDouble(floorHeightFixed);
    ceilingHeight = FixedToDouble(ceilHeightFixed);

    serialization::deserialize(r, floorPic);
    serialization::deserialize(r, ceilingPic);
    serialization::deserialize(r, lightLevel);
    serialization::deserialize(r, specialType);
    serialization::deserialize(r, tag);
  }
};

struct EditorLevel
{
  EditorLevel() = default;
  EditorLevel(std::vector<EditorVertex> _vertices,
              std::vector<EditorLineDef> _lines,
              std::vector<EditorSector> _sectors,
              std::vector<EditorSidedef> _sidedefs,
              const size_t _levelNum,
              const std::array<char8_t, 8> _name)
    : vertices(std::move(_vertices)), linedefs(std::move(_lines)), sectors(std::move(_sectors)),
      sidedefs(std::move(_sidedefs)), levelNum(_levelNum), name(_name)
  {}
  explicit EditorLevel(const size_t _levelNum, const std::array<char8_t, 8> _name) : levelNum(_levelNum), name(_name)
  {}
  EditorLevel(const Level &_level, uint16_t _levelNum);

  void SerializeLevelToBuffer(const std::unique_ptr<BspLevel> &bspLevel, std::vector<uint8_t> &buffer) const;
  void SaveLevelToFile(uint16_t &numberOfLevels, const std::unique_ptr<BspLevel> &bspLevel) const;
  void Save(uint16_t &numberOfLevels);
  void Load(uint16_t width, uint16_t height);

  static size_t Load(std::unique_ptr<Level> &level);

  std::vector<EditorVertex> vertices;
  std::vector<EditorLineDef> linedefs;
  std::vector<EditorSector> sectors;
  std::vector<EditorSidedef> sidedefs;
  // equals to zero when level is the first lump
  uint16_t levelNum = 0;
  std::array<char8_t, 8> name{};
};

struct EditorState
{
  EditorState(Level &_level, uint16_t _levelNum, uint16_t _numOfLevels);
  void Reset();

  uint16_t numOfLevels = 0;
  // information about the linedefs/sidedefs/vertices
  std::unique_ptr<EditorLevel> level;
  TextureManager *texManager;
  PaletteManager *palManager;

  // dragging state
  bool isDragging = false;
  bool isShiftDragging = false;
  ImVec2 shiftDraggingStart = { 0, 0 };
  ImVec2 draggingOffset = { 0, 0 };
  ImVec2 draggingStart = { 0, 0 };
  int shiftDraggingAxis = 0;// 0 = none, 1 = x, 2 = y

  // block selection state
  bool isBlockSelecting = false;
  ImVec2 blockSelectionStart = { 0, 0 };
  ImVec2 blockSelectionOffset = { 0, 0 };

  // line creation state
  bool isCreatingLine = false;
  uint32_t lineStartVertexId = 0;

  // options window state
  bool renderOptionsWindow = false;
  ImVec2 optionsWindowPos = { 0, 0 };

  std::set<uint32_t> selection;
  std::set<uint32_t> dragged;
  uint32_t hoveredObjectId = UINT32_MAX;

  std::vector<ImVec2> transformedVertices;

  bool isScrolling = false;
  ImVec2 scrollingStart = { 0.0, 0.0 };
  ImVec2 scrollingOffset = { 0.0, 0.0 };

  float canvasZoom = 1.0f;

  [[nodiscard]] EditorVertex &findVertex(const uint32_t id) const
  {
    const uint32_t index = getObjectIndex(id);

    if (index >= level->vertices.size()) {
      throw std::runtime_error("Index is bigger than the sectors array");
    }

    return level->vertices[index];
  }

  [[nodiscard]] ImVec2 findTransformedVertex(const uint32_t id) const
  {
    const uint32_t index = getObjectIndex(id);

    if (index >= transformedVertices.size()) {
      return { 0, 0 };
    }

    return transformedVertices[index];
  }

  [[nodiscard]] EditorSector &findSector(const uint32_t id) const
  {
    const uint32_t index = getObjectIndex(id);
    if (index >= level->sectors.size()) {
      throw std::runtime_error("Index is bigger than the sectors array.");
    }

    return level->sectors[index];
  }

  [[nodiscard]] EditorLineDef &findLinedef(const uint32_t id) const
  {
    uint32_t index = getObjectIndex(id);

    if (index >= level->linedefs.size()) {
      throw std::runtime_error("Index is bigger than the linedefs array.");
    }

    return level->linedefs[index];
  }

  [[nodiscard]] EditorObject *findObject(const uint32_t objectId) const
  {
    const auto type = getObjectType(objectId);
    const auto index = getObjectIndex(objectId);

    switch (type) {
    case EditorObjectType::VERTEX:
      if (index < level->vertices.size())
        return &level->vertices[index];
      break;
    case EditorObjectType::LINEDEF:
      if (index < level->linedefs.size())
        return &level->linedefs[index];
      break;
    case EditorObjectType::SECTOR:
      if (index < level->sectors.size())
        return &level->sectors[index];
      break;
    default:
      break;
    }

    return nullptr;
  }
};


#include "editor_input_handler.h"
#include "editor_renderer.h"

class Editor
{
private:
  std::shared_ptr<CommandHistory> m_history;
  std::unique_ptr<EditorInputHandler> m_inputHandler;
  std::unique_ptr<EditorRenderer> m_renderer;

private:
  void updateAABB(uint32_t sectorID) const;

public:
  std::shared_ptr<EditorState> state;

public:
  Editor(std::shared_ptr<SdlWindow> sdlWindow,
         Level &_level,
         uint16_t _levelNum,
         uint16_t _numOfLevels,
         TextureManager &textureManager,
         PaletteManager &palManager);
  ~Editor() = default;

  void Render() const;
  [[nodiscard]] std::array<char8_t, 8> GetLevelName() const;

  void resetStateFrame() const;

  void ProcessInput(float_t vertexRadius) const;
  void executeCommand(std::unique_ptr<Command> cmd) const;
  void addLineDef(int32_t sectorId, LineDef &linedef) const;
  void addVertex(double x, double y) const;
  void drawConnectedLine(uint32_t vertexId) const;

  void TransformVertices() const;
  [[nodiscard]] ImVec2 TransformVertex(const EditorVertex &v) const;
  [[nodiscard]] ImVec2 UntransformVertex(ImVec2 transformed) const;

  void AddEmptyLevel() const;
  void ChangeLevel(uint16_t newLevelNum) const;


  template<HasXY T>
  static constexpr T scale(const T &vec, const float scaleFactor)
  {
    return { decltype(vec.x)(scaleFactor * static_cast<float>(vec.x)),
             decltype(vec.y)(scaleFactor * static_cast<float>(vec.y)) };
  }

  template<HasXY T>
  static constexpr T unscale(const T &vec, const float scaleFactor)
  {
    assert(scaleFactor != 0);

    return { decltype(vec.x)(static_cast<float>(vec.x) / scaleFactor),
             decltype(vec.y)(static_cast<float>(vec.y) / scaleFactor) };
  }
};
