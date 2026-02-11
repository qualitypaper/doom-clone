#pragma once

#include "gameloop.h"
#include "imgui.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <stacktrace>
#include <vector>

// forward declarations
namespace commands {
struct CommandHistory;
struct Command;
}// namespace commands

namespace editor {

// forward declaractions
struct EditorState;
struct EditorInputHandler;

struct AABB
{
  AABB() = default;
  AABB(gameloop::Vertex start, gameloop::Vertex end);
  int16_t maxX, maxY;
  int16_t minX, minY;

  bool contains(int16_t x, int16_t y) { return x >= minX && x <= maxX && y >= minY && y <= maxY; }
};

enum class EditorObjectType { LINEDEF, VERTEX, SECTOR };

// Encoded object id: top 2 bits store type, remaining bits store index
constexpr uint32_t kObjectTypeShift = 30;
constexpr uint32_t kObjectIndexMask = (1u << kObjectTypeShift) - 1u;

constexpr uint32_t makeObjectId(EditorObjectType type, uint32_t index)
{ return (static_cast<uint32_t>(type) << kObjectTypeShift) | (index & kObjectIndexMask); }

constexpr EditorObjectType getObjectType(uint32_t objectId)
{ return static_cast<EditorObjectType>((objectId >> kObjectTypeShift) & 0x3u); }

constexpr uint32_t getObjectIndex(uint32_t objectId) { return objectId & kObjectIndexMask; }

struct EditorObject
{
  EditorObject(EditorObjectType _type) : type(_type) {}
  virtual ~EditorObject() = default;

  EditorObjectType type;
  bool selected = false;
  bool hovered = false;
};

struct EditorLineDef : EditorObject
{
  EditorLineDef(const gameloop::LineDef &_linedef)
    : EditorObject(EditorObjectType::LINEDEF), start(_linedef.start), end(_linedef.end), type(_linedef.type),
      frontSideDef(_linedef.frontSidedef), backSideDef(_linedef.backSidedef)
  {}
  EditorLineDef(const uint32_t _start,
    const uint32_t _end,
    const gameloop::LineDefType _type,
    const uint32_t _frontSideDef,
    const uint32_t _backSideDef)
    : EditorObject(EditorObjectType::LINEDEF), start(_start), end(_end), type(_type), frontSideDef(_frontSideDef),
      backSideDef(_backSideDef)
  {}
  uint32_t start;
  uint32_t end;
  gameloop::LineDefType type;
  int32_t frontSideDef;
  int32_t backSideDef;

  static void remove(const EditorState &state, uint32_t ldId);
};

struct EditorVertex : EditorObject
{
  EditorVertex(const int32_t _x, const int32_t _y) : EditorObject(EditorObjectType::VERTEX), x(_x), y(_y) {}

  int32_t x, y;
  std::vector<uint32_t> connectedLineDefs;// Linedef object IDs (EditorObjectType::LINEDEF).

  [[nodiscard]] bool isAnyConnectedLineDefSelected(const EditorState &state) const;
  // immutable add
  [[nodiscard]] EditorVertex add(const EditorVertex &other) const { return { x + other.x, y + other.y }; }
  [[nodiscard]] EditorVertex add(const ImVec2 &other) const
  {
    return { static_cast<int32_t>(static_cast<float_t>(x) + other.x),
      static_cast<int32_t>(static_cast<float_t>(y) + other.y) };
  }

  constexpr EditorVertex &operator+=(const EditorVertex &other)
  {
    x += other.x;
    y += other.y;
    return *this;
  }

  constexpr EditorVertex &operator+=(const ImVec2 &offset)
  {
    x += static_cast<int32_t>(offset.x);
    y += static_cast<int32_t>(offset.y);

    return *this;
  }
  [[nodiscard]] constexpr ImVec2 toImVec2() const { return { static_cast<float_t>(x), static_cast<float_t>(y) }; }
  static void remove(EditorState &state, uint32_t vertexId);
};
constexpr EditorVertex &operator+(EditorVertex &lhs, const EditorVertex &rhs) noexcept
{
  lhs += rhs;
  return lhs;
}
constexpr EditorVertex &operator+(EditorVertex &lhs, const ImVec2 &rhs) noexcept
{
  lhs += rhs;
  return lhs;
}

struct EditorSector : EditorObject
{
  EditorSector() : EditorObject(EditorObjectType::SECTOR) {}

  int16_t floorHeight = 0;
  int16_t ceilingHeight = 0;
  int16_t specialType = 0;
  int16_t lightLevel = 0;
  int16_t tag = 0;
  AABB bounding_box;
  std::vector<uint32_t> linedefIds;
};

struct EditorLevel
{
  EditorLevel(std::vector<EditorVertex> _vertices,
    std::vector<EditorLineDef> _lines,
    std::vector<EditorSector> _sectors,
    std::vector<gameloop::SideDef> &_sidedefs)
    : vertices(std::move(_vertices)), linedefs(std::move(_lines)), sectors(std::move(_sectors)), sidedefs(_sidedefs)
  {}

  std::vector<EditorVertex> vertices;
  std::vector<EditorLineDef> linedefs;
  std::vector<EditorSector> sectors;
  std::vector<gameloop::SideDef> &sidedefs;
};

struct EditorState
{
  EditorState(gameloop::Level &_level, uint16_t width, uint16_t height);
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
    if (id >= level->vertices.size()) {
      throw std::runtime_error("Index is bigger than the vertices array. Index: " + std::to_string(id));
    }

    return level->vertices[id];
  }

  [[nodiscard]] EditorSector &findSector(const uint32_t id) const
  {
    if (id >= level->sectors.size()) { throw std::runtime_error("Index is bigger than the sectors array."); }

    return level->sectors[id];
  }

  [[nodiscard]] EditorLineDef &findLinedef(const uint32_t id) const
  {
    if (id >= level->linedefs.size()) { throw std::runtime_error("Index is bigger than the linedefs array."); }

    return level->linedefs[id];
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
  std::unique_ptr<commands::CommandHistory> m_history;

private:
  void addToSector(uint16_t i, gameloop::LineDef &linedef, gameloop::SideDef &sidedef);
  void updateAABB(uint32_t sectorID);

public:
  std::unique_ptr<EditorState> state;

public:
  Editor(gameloop::Level &_level, uint16_t _width, uint16_t _height);

  void processInput(float_t vertexRadius) const;
  void executeCommand(std::unique_ptr<commands::Command> cmd) const;
  void addLineDef(gameloop::Vertex start, gameloop::Vertex end);
  void addLineDef(int32_t sectorId, gameloop::LineDef &lineDef);
  void addVertex(int16_t x, int16_t y) const;
  void drawConnectedLine(uint32_t vertexIndex) const;
};

}// namespace editor
