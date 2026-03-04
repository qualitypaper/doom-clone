#pragma once

#include "gameloop.h"
#include "imgui.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <serialization.h>
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
  { return x >= minX && x <= maxX && y >= minY && y <= maxY; }
};

enum class EditorObjectType { LINEDEF, VERTEX, SECTOR, SIDEDEF };

// Encoded object id: top 2 bits store type, remaining bits store index
constexpr uint32_t kObjectTypeShift = 30;
constexpr uint32_t kObjectIndexMask = (1u << kObjectTypeShift) - 1u;

constexpr uint32_t makeObjectId(EditorObjectType type, uint32_t index)
{ return (static_cast<uint32_t>(type) << kObjectTypeShift) | (index & kObjectIndexMask); }

constexpr EditorObjectType getObjectType(uint32_t objectId)
{ return static_cast<EditorObjectType>((objectId >> kObjectTypeShift) & 0x3u); }

constexpr uint32_t getObjectIndex(uint32_t objectId) { return objectId & kObjectIndexMask; }

struct EditorObject : Serializable
{
  explicit EditorObject(const EditorObjectType _type) : type(_type) {}
  ~EditorObject() override = default;

  EditorObjectType type;
  bool selected = false;
  bool hovered = false;
};

struct EditorLineDef : EditorObject
{
  EditorLineDef() : EditorObject(EditorObjectType::LINEDEF) {}
  explicit EditorLineDef(const LineDef &_linedef)
    : EditorObject(EditorObjectType::LINEDEF), start(_linedef.start), end(_linedef.end), type(_linedef.type),
      frontSideDef(_linedef.frontSidedef), backSideDef(_linedef.backSidedef)
  {}
  EditorLineDef(const uint32_t _start,
    const uint32_t _end,
    const LineDefType _type,
    const uint32_t _frontSideDef,
    const uint32_t _backSideDef)
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
};

struct EditorVertex : EditorObject
{
  EditorVertex() : EditorObject(EditorObjectType::VERTEX) {}
  EditorVertex(const int32_t _x, const int32_t _y) : EditorObject(EditorObjectType::VERTEX), x(_x), y(_y) {}
  explicit EditorVertex(const ImVec2 vec)
    : EditorObject(EditorObjectType::VERTEX), x(static_cast<int32_t>(vec.x)), y(static_cast<int32_t>(vec.y))
  {}

  int32_t x = 0, y = 0;
  std::vector<uint32_t> connectedLineDefs = {};// Linedef object IDs (EditorObjectType::LINEDEF).

  EditorVertex operator+(const EditorVertex &other) const { return { x + other.x, y + other.y }; }
  EditorVertex operator-(const EditorVertex &other) const { return { x - other.x, y - other.y }; }

  EditorVertex operator+(const ImVec2 &other) const
  { return { x + static_cast<int32_t>(other.x), y + static_cast<int32_t>(other.y) }; }
  EditorVertex operator-(const ImVec2 &other) const
  { return { x - static_cast<int32_t>(other.x), y - static_cast<int32_t>(other.y) }; }

  EditorVertex operator*(const double c) const { return { static_cast<int32_t>(x * c), static_cast<int32_t>(y * c) }; }
  EditorVertex operator/(const double c) const { return { static_cast<int32_t>(x / c), static_cast<int32_t>(y / c) }; }

  [[nodiscard]] bool isAnyConnectedLineDefSelected(const EditorState &state) const;
  [[nodiscard]] constexpr ImVec2 toImVec2() const { return { static_cast<float_t>(x), static_cast<float_t>(y) }; }
  [[nodiscard]] constexpr double length() const { return std::sqrt(x * x + y * y); }
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
  int16_t specialType = 0;
  int16_t lightLevel = 0;
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
    std::vector<EditorSidedef> _sidedefs)
    : vertices(std::move(_vertices)), linedefs(std::move(_lines)), sectors(std::move(_sectors)),
      sidedefs(std::move(_sidedefs))
  {}


  void serialize(const std::filesystem::path &path) const;
  void deserialize(const std::filesystem::path &path);

  void toGameLevel(Level &level, uint16_t width, uint16_t height) const;

  std::vector<EditorVertex> vertices;
  std::vector<EditorLineDef> linedefs;
  std::vector<EditorSector> sectors;
  std::vector<EditorSidedef> sidedefs;
};

struct EditorState
{
  EditorState(Level &_level, uint16_t _width, uint16_t _height);
  void reset();

  uint16_t width, height;

  // information about the linedefs/sidedefs/vertices
  std::unique_ptr<EditorLevel> level;

  // dragging state
  bool isDragging = false;
  ImVec2 draggingOffset = { 0, 0 };
  ImVec2 draggingStart = { 0, 0 };

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

  std::vector<uint32_t> selection;

  ImVec2 canvasOrigin = { 0.0, 0.0 };
  ImVec2 canvasScroll = { 0.0, 0.0 };

  float canvasZoom = 1.0f;

  [[nodiscard]] EditorVertex &findVertex(const uint32_t id) const
  {
    const uint32_t index = getObjectIndex(id);

    if (index >= level->vertices.size()) { throw std::runtime_error("Index is bigger than the sectors array"); }

    return level->vertices[index];
  }

  [[nodiscard]] EditorSector &findSector(const uint32_t id) const
  {
    const uint32_t index = getObjectIndex(id);
    if (index >= level->sectors.size()) { throw std::runtime_error("Index is bigger than the sectors array."); }

    return level->sectors[index];
  }

  [[nodiscard]] EditorLineDef &findLinedef(const uint32_t id) const
  {
    uint32_t index = getObjectIndex(id);

    if (index >= level->linedefs.size()) { throw std::runtime_error("Index is bigger than the linedefs array."); }

    return level->linedefs[index];
  }

  [[nodiscard]] EditorObject *findObject(const uint32_t objectId) const
  {
    const auto type = getObjectType(objectId);
    const auto index = getObjectIndex(objectId);

    switch (type) {
    case EditorObjectType::VERTEX:
      if (index < level->vertices.size()) return &level->vertices[index];
      break;
    case EditorObjectType::LINEDEF:
      if (index < level->linedefs.size()) return &level->linedefs[index];
      break;
    case EditorObjectType::SECTOR:
      if (index < level->sectors.size()) return &level->sectors[index];
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
  std::unique_ptr<EditorInputHandler> m_inputHandler;
  std::unique_ptr<CommandHistory> m_history;

private:
  void updateAABB(uint32_t sectorID) const;

public:
  std::unique_ptr<EditorState> state;

public:
  Editor(Level &_level, uint16_t _width, uint16_t _height);

  void processInput(float_t vertexRadius) const;
  void executeCommand(std::unique_ptr<Command> cmd) const;
  void addLineDef(int32_t sectorId, LineDef &linedef) const;
  void addVertex(int16_t x, int16_t y) const;
  void drawConnectedLine(uint32_t vertexIndex) const;
};
