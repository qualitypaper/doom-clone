#pragma once

#include "gameloop.h"
#include "imgui.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

// forward declarations
namespace commands {
struct CommandHistory;
}

namespace editor {

// forward declaractions
struct EditorInputHandler;

struct AABB
{
  AABB() = default;
  AABB(gameloop::Vertex start, gameloop::Vertex end);
  int16_t maxX, maxY;
  int16_t minX, minY;

  bool contains(int16_t x, int16_t y) { return x >= minX && x <= maxX && y >= minY && y <= maxY; }
};

struct EditorObject
{
  EditorObject(uint32_t _id) : id(_id) {}
  virtual ~EditorObject() = default;

  uint32_t id;
  bool selected = false;
  bool hovered = false;
};

struct EditorLineDef : EditorObject
{
  EditorLineDef(uint32_t _id,
    uint32_t _start,
    uint32_t _end,
    gameloop::LineDefType _type,
    uint32_t _frontSidedef,
    uint32_t _backSidedef)
    : EditorObject(_id), start(_start), end(_end), type(_type), frontSidedef(_frontSidedef), backSidedef(_backSidedef)
  {}
  uint32_t start;
  uint32_t end;
  gameloop::LineDefType type;
  int16_t frontSidedef;
  int16_t backSidedef;
};

struct EditorVertex : EditorObject
{
  EditorVertex(uint32_t _id, int32_t _x, int32_t _y) : EditorObject(_id), x(_x), y(_y) {}

  int32_t x, y;

  constexpr ImVec2 toImVec2() const { return ImVec2(x, y); }
};

struct EditorSector : EditorObject
{
  EditorSector(uint32_t _id) : EditorObject(_id) {}

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

  uint16_t width, height;

  // information about the linedefs/sidedefs/vertices
  std::unique_ptr<EditorLevel> level;
  std::unordered_map<uint32_t, EditorObject &> objects;
  uint32_t nextId = 1;

  // dragging logic
  bool isDragging = false;
  ImVec2 draggingOffset = { 0, 0 };

  bool isCreatingLine = false;
  uint32_t lineStartVertexId = 0;

  bool renderOptionsWindow = false;
  ImVec2 optionsWindowPos = { 0, 0 };

  std::vector<uint32_t> selection;

  ImVec2 canvasOrigin = { 0.0, 0.0 };
  ImVec2 canvasScroll = { 0.0, 0.0 };
  float canvasZoom = 1.0f;

  uint32_t getNextId() { return nextId++; }

  EditorVertex *findVertex(uint32_t id)
  {
    for (auto &v : level.get()->vertices) {
      if (v.id == id) return &v;
    }

    return nullptr;
  }

  EditorSector *findSector(uint32_t id)
  {
    for (auto &s : level.get()->sectors) {
      if (s.id == id) return &s;
    }

    return nullptr;
  }

  EditorLineDef *findLinedef(uint32_t id)
  {
    for (auto &ld : level.get()->linedefs) {
      if (ld.id == id) return &ld;
    }

    return nullptr;
  }

  EditorObject *findObject(uint32_t id)
  {
    auto it = objects.find(id);
    if (it != objects.end()) return &it->second;

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
  void addLineDef(gameloop::Vertex start, gameloop::Vertex end);
  void addLineDef(int32_t sectorId, gameloop::LineDef &lineDef);
  void addVertex(int32_t sectorId, gameloop::Vertex &vertex);
};

}// namespace editor
