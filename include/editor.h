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
      frontSidedef(_linedef.frontSidedef), backSidedef(_linedef.backSidedef)
  {}
  EditorLineDef(uint32_t _start,
    uint32_t _end,
    gameloop::LineDefType _type,
    uint32_t _frontSidedef,
    uint32_t _backSidedef)
    : EditorObject(EditorObjectType::LINEDEF), start(_start), end(_end), type(_type), frontSidedef(_frontSidedef),
      backSidedef(_backSidedef)
  {}
  uint32_t start;
  uint32_t end;
  gameloop::LineDefType type;
  int32_t frontSidedef;
  int32_t backSidedef;

  static void remove(const EditorState &state, uint32_t ldId);
};

struct EditorVertex : EditorObject
{
  EditorVertex(int32_t _x, int32_t _y) : EditorObject(EditorObjectType::VERTEX), x(_x), y(_y) {}

  int32_t x, y;
  std::vector<uint32_t> connectedLineDefs;// Linedef object IDs (EditorObjectType::LINEDEF).

  bool isAnyConnectedLineDefSelected(const EditorState &state) const;

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
  constexpr ImVec2 toImVec2() const { return ImVec2(x, y); }
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

  int16_t floorHeight;
  int16_t ceilingHeight;
  int16_t specialType;
  int16_t lightLevel;
  int16_t tag;
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
  EditorState(std::unique_ptr<EditorLevel> _level) : level(std::move(_level)) {}
  EditorState(gameloop::Level &_level, uint16_t width, uint16_t height);
  void reset();

  uint16_t width, height;

  // information about the linedefs/sidedefs/vertices
  std::unique_ptr<EditorLevel> level;

  // dragging logic
  bool isDragging = false;
  ImVec2 draggingOffset = { 0, 0 };
  ImVec2 draggingStart = { 0, 0 };

  bool isCreatingLine = false;
  uint32_t lineStartVertexId = 0;

  bool renderOptionsWindow = false;
  ImVec2 optionsWindowPos = { 0, 0 };

  std::vector<uint32_t> selection;

  ImVec2 canvasOrigin = { 0.0, 0.0 };
  ImVec2 canvasScroll = { 0.0, 0.0 };
  float canvasZoom = 1.0f;

  EditorVertex &findVertex(uint32_t id) const
  {
    if (id >= level->vertices.size()) {
      throw std::runtime_error("Index is bigger than the vertices array. Index: " + std::to_string(id));
    }

    return level->vertices[id];
  }

  EditorSector &findSector(uint32_t id) const
  {
    if (id >= level->sectors.size()) { throw std::runtime_error("Index is bigger than the sectors array."); }

    return level->sectors[id];
  }

  EditorLineDef &findLinedef(uint32_t id) const
  {
    if (id >= level->linedefs.size()) { throw std::runtime_error("Index is bigger than the linedefs array."); }

    return level->linedefs[id];
  }

  EditorObject *findObject(uint32_t objectId)
  {
    auto type = getObjectType(objectId);
    auto index = getObjectIndex(objectId);

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
  std::unique_ptr<editor::EditorInputHandler> m_inputHandler;
  std::unique_ptr<commands::CommandHistory> m_history;

private:
  void addToSector(uint16_t i, gameloop::LineDef &linedef, gameloop::SideDef &sidedef);
  void updateAABB(uint32_t sectorID);

public:
  std::unique_ptr<editor::EditorState> state;

public:
  Editor(gameloop::Level &_level, uint16_t _width, uint16_t _height);

  void processInput();
  void executeCommand(std::unique_ptr<commands::Command> cmd);
  void addLineDef(gameloop::Vertex start, gameloop::Vertex end);
  void addLineDef(int32_t sectorId, gameloop::LineDef &lineDef);
  void addVertex(int32_t sectorId, int16_t x, int16_t y);
};

}// namespace editor
