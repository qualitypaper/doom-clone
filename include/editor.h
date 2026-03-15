#pragma once

#include "gameloop.h"
#include "imgui.h"
#include "math_utils.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <serialization.h>
#include <set>
#include <vector>

// forward declarations
struct CommandHistory;
struct Command;

// forward declaractions
struct EditorState;
struct EditorInputHandler;

struct AABB
{
  AABB() = default;
  AABB(Vertex start, Vertex end);
  int16_t maxX, maxY;
  int16_t minX, minY;

  [[nodiscard]] bool contains(const int16_t x, const int16_t y) const
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

constexpr uint32_t getObjectIndex(const uint32_t objectId) { return objectId & kObjectIndexMask; }

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
  explicit EditorObject(const EditorObjectType _type) : type(_type) {}
  ~EditorObject() override = default;

  EditorObjectType type;
  bool selected = false;
  bool hovered = false;

  virtual void select() { this->selected = !this->selected; }

  virtual void hover() { this->hovered = !this->hovered; }
};

struct EditorLineDef : public EditorObject
{
  EditorLineDef() : EditorObject(EditorObjectType::LINEDEF) {}
  explicit EditorLineDef(const LineDef &_linedef)
    : EditorObject(EditorObjectType::LINEDEF), start(makeObjectId(EditorObjectType::VERTEX, _linedef.start)),
      end(makeObjectId(EditorObjectType::VERTEX, _linedef.end)), type(_linedef.type),
      frontSideDef(_linedef.frontSidedef), backSideDef(_linedef.backSidedef)
  {}
  EditorLineDef(const uint32_t _start,
    const uint32_t _end,
    const LineDefType _type,
    const int32_t _frontSideDef,
    const int32_t _backSideDef)
    : EditorObject(EditorObjectType::LINEDEF), start(_start), end(_end), type(_type), frontSideDef(_frontSideDef),
      backSideDef(_backSideDef)
  {}

  uint32_t start = 0;
  uint32_t end = 0;
  LineDefType type = LineDefType::REGULAR;
  int32_t frontSideDef = -1;
  int32_t backSideDef = -1;

  static void remove(const EditorState &state, uint32_t ldId);

  void serialize(FileWriter &fw) const override;
  void deserialize(FileReader &fr) override;

  void drag(uint32_t objectId, EditorState *state, CommandHistory *history) override;
};

struct EditorVertex : public EditorObject
{
  EditorVertex() : EditorObject(EditorObjectType::VERTEX) {}
  EditorVertex(const int32_t _x, const int32_t _y) : EditorObject(EditorObjectType::VERTEX), x(_x), y(_y) {}
  explicit EditorVertex(const ImVec2 &vec)
    : EditorObject(EditorObjectType::VERTEX), x(static_cast<int32_t>(vec.x)), y(static_cast<int32_t>(vec.y))
  {}
  explicit EditorVertex(const ImVec2 vec)
    : EditorObject(EditorObjectType::VERTEX), x(static_cast<int32_t>(vec.x)), y(static_cast<int32_t>(vec.y))
  {}

  int32_t x = 0, y = 0;
  std::vector<uint32_t> connectedLineDefs = {};// Linedef object IDs (EditorObjectType::LINEDEF).

  EditorVertex operator+(const EditorVertex &other) const { return { x + other.x, y + other.y }; }
  EditorVertex operator-(const EditorVertex &other) const { return { x - other.x, y - other.y }; }

  EditorVertex operator+(const ImVec2 &other) const
  {
    return { x + static_cast<int32_t>(other.x), y + static_cast<int32_t>(other.y) };
  }
  EditorVertex operator-(const ImVec2 &other) const
  {
    return { x - static_cast<int32_t>(other.x), y - static_cast<int32_t>(other.y) };
  }

  EditorVertex operator*(const double c) const { return { static_cast<int32_t>(x * c), static_cast<int32_t>(y * c) }; }
  EditorVertex operator/(const double c) const { return { static_cast<int32_t>(x / c), static_cast<int32_t>(y / c) }; }

  EditorVertex &operator+=(const ImVec2 &offset)
  {
    this->x += static_cast<int32_t>(offset.x);
    this->y += static_cast<int32_t>(offset.y);

    return *this;
  }
  EditorVertex &operator-=(const ImVec2 &offset)
  {
    this->x -= static_cast<int32_t>(offset.x);
    this->y -= static_cast<int32_t>(offset.y);

    return *this;
  }

  [[nodiscard]] bool isAnyConnectedLineDefSelected(const EditorState &state) const;
  [[nodiscard]] ImVec2 toImVec2() const { return { static_cast<float_t>(x), static_cast<float_t>(y) }; }
  [[nodiscard]] double length() const { return std::sqrt(x * x + y * y); }
  [[nodiscard]] ImVec2 fromCenterCoords(const SdlWindow &sdlWindow) const;

  void normalize()
  {
    const double len = length();

    x = static_cast<int32_t>(std::round(static_cast<double>(x) / len));
    y = static_cast<int32_t>(std::round(static_cast<double>(y) / len));
  }

  static void remove(EditorState &state, uint32_t vertexId);

  void serialize(FileWriter &fw) const override;
  void deserialize(FileReader &fr) override;

  void drag(uint32_t objectId, EditorState *state, CommandHistory *history) override;
};

struct EditorSidedef : EditorObject
{
  EditorSidedef(int16_t _sectorId, int16_t _xOffset, int16_t _yOffset)
    : EditorObject(EditorObjectType::SIDEDEF), sectorId(_sectorId), xOffset(_xOffset), yOffset(_yOffset)
  {}
  EditorSidedef() : EditorObject(EditorObjectType::SIDEDEF) {}

  int16_t sectorId = -1;
  int16_t xOffset = 0;
  int16_t yOffset = 0;
  int16_t upperWallTexture = -1;
  int16_t middleWallTexture = -1;
  int16_t bottomWallTexture = -1;


  void serialize(FileWriter &fw) const override;
  void deserialize(FileReader &fr) override;
};

struct EditorSector : EditorObject
{
  EditorSector(int16_t _floorHeight,
    int16_t _ceilingHeight,
    int16_t _specialType,
    int16_t _lightLevel,
    int16_t _tag,
    uint32_t _color = 0)
    : EditorObject(EditorObjectType::SECTOR), floorHeight(_floorHeight), ceilingHeight(_ceilingHeight),
      specialType(_specialType), lightLevel(_lightLevel), tag(_tag), color(_color)
  {}
  EditorSector() : EditorObject(EditorObjectType::SECTOR) {}

  int16_t floorHeight = 0;
  int16_t ceilingHeight = 0;
  int16_t floorTextureIndex = -1;
  int16_t ceilingTextureIndex = -1;
  int16_t lightLevel = 0;
  int16_t specialType = 0;
  int16_t tag = 0;
  uint32_t color = 0;
  AABB bounding_box{};
  std::vector<uint32_t> linedefIds;

  void serialize(FileWriter &fw) const override;
  void deserialize(FileReader &fr) override;
};

struct EditorLevel
{
  EditorLevel() = default;
  EditorLevel(std::vector<EditorVertex> _vertices,
    std::vector<EditorLineDef> _lines,
    std::vector<EditorSector> _sectors,
    std::vector<EditorSidedef> _sidedefs,
    size_t _levelNum)
    : vertices(std::move(_vertices)), linedefs(std::move(_lines)), sectors(std::move(_sectors)),
      sidedefs(std::move(_sidedefs)), levelNum(_levelNum)
  {}


  void save(uint16_t &levelsNum, const uint16_t width, const uint16_t height);
  void Load(const uint16_t width, const uint16_t height);
  void toGameLevel(Level &level, uint16_t width, uint16_t height) const;
  static void Load(Level &level, const uint16_t levelNum);
  static void SkipHeaderAndLevels(FileReader &fr, const uint16_t levelNum);
  static void SkipLevels(FileReader &fr, uint16_t levelNum);


  std::vector<EditorVertex> vertices;
  std::vector<EditorLineDef> linedefs;
  std::vector<EditorSector> sectors;
  std::vector<EditorSidedef> sidedefs;
  // equals to zero when level is the first lump
  uint16_t levelNum;
};

struct EditorState
{
  EditorState(Level &_level, const uint16_t _levelNum, const uint16_t _numOfLevels, uint16_t _width, uint16_t _height);
  void reset();

  uint16_t width, height;

  uint16_t numOfLevels = 0;
  // information about the linedefs/sidedefs/vertices
  std::unique_ptr<EditorLevel> level;

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

    if (index >= level->vertices.size()) {
      throw std::runtime_error("Index is bigger than the sectors array");
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

class Editor
{
private:
  std::shared_ptr<CommandHistory> m_history;
  std::unique_ptr<EditorInputHandler> m_inputHandler;

private:
  void updateAABB(uint32_t sectorID) const;

public:
  std::shared_ptr<EditorState> state;

public:
  Editor(Level &_level,
    const uint16_t _levelNum,
    const uint16_t _numOfLevels,
    const uint16_t _width,
    const uint16_t _height);

  void resetStateFrame() const;

  void processInput(float_t vertexRadius) const;
  void executeCommand(std::unique_ptr<Command> cmd) const;
  void addLineDef(int32_t sectorId, LineDef &linedef) const;
  void addVertex(int32_t x, int32_t y) const;
  void drawConnectedLine(uint32_t vertexIndex) const;

  void transformVertices() const;
  [[nodiscard]] ImVec2 transformVertex(const EditorVertex &v) const;
  [[nodiscard]] ImVec2 untransformVertex(ImVec2 transformed) const;

  template<HasXY T> static constexpr T scale(const T &vec, const float scaleFactor)
  {
    return { decltype(vec.x)(scaleFactor * static_cast<float>(vec.x)),
      decltype(vec.y)(scaleFactor * static_cast<float>(vec.y)) };
  }

  template<HasXY T> static constexpr T unscale(const T &vec, const float scaleFactor)
  {
    assert(scaleFactor != 0);

    return { decltype(vec.x)(static_cast<float>(vec.x) / scaleFactor),
      decltype(vec.y)(static_cast<float>(vec.y) / scaleFactor) };
  }

  // zooms the vertex
  // if no zoom parameter is specified will default to state->canvasZoom
  template<HasXY T> ImVec2 zoomVertex(const T &vertex, float zoom = -1) const
  {
    if (zoom == -1) {
      zoom = state->canvasZoom;
    }
    T centered = math_utils::toCenterCoordinates(vertex, state->width, state->height);

    centered = Editor::scale(centered, zoom);
    T from_center_coordinates = math_utils::fromCenterCoordinates(centered, state->width, state->height);

    return { static_cast<float>(from_center_coordinates.x), static_cast<float>(from_center_coordinates.y) };
  }
};
