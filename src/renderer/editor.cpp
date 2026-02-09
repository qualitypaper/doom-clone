#include "editor.h"

#include "commands.h"
#include "editor_input_handler.h"
#include "gameloop.h"
#include "math_utils.h"

#include "algorithm"
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace editor {

AABB::AABB(gameloop::Vertex start, gameloop::Vertex end)
{
  this->maxX = std::max(start.x, end.x);
  this->minX = std::min(start.x, end.x);
  this->maxY = std::max(start.y, end.y);
  this->minY = std::min(start.y, end.y);
}

bool EditorVertex::isAnyConnectedLineDefSelected(const EditorState &state) const
{
  return std::any_of(connectedLineDefs.begin(), connectedLineDefs.end(), [&](auto ldObjectId) {
    return state.findLinedef(getObjectIndex(ldObjectId)).selected;
  });
}

EditorState::EditorState(gameloop::Level &_level, uint16_t _width, uint16_t _height) : width(_width), height(_height)
{
  std::vector<EditorVertex> editorVertices;
  std::vector<EditorLineDef> editorLinedefs;
  std::vector<EditorSector> editorSectors;

  editorVertices.reserve(_level.vertices.size());
  editorLinedefs.reserve(_level.linedefs.size());
  editorSectors.reserve(_level.sectors.size());

  std::unordered_map<int16_t, uint32_t> vertexIdMap;
  vertexIdMap.reserve(_level.vertices.size());

  // process vertices
  for (uint16_t i = 0; i < _level.vertices.size(); ++i) {
    auto &v = _level.vertices[i];

    auto convertedImvec2 = math_utils::fromCenterCoordinates(math_utils::convertVertexIntoImVec2(v), _width, _height);
    editorVertices.emplace_back(convertedImvec2.x, convertedImvec2.y);
    vertexIdMap[i] = editorVertices.size() - 1;
  }

  // process linedefs
  for (auto &ld : _level.linedefs) {
    editorLinedefs.emplace_back(vertexIdMap[ld.start], vertexIdMap[ld.end], ld.type, ld.frontSidedef, ld.backSidedef);
    editorVertices[vertexIdMap[ld.start]].connectedLineDefs.emplace_back(
      makeObjectId(EditorObjectType::LINEDEF, static_cast<uint32_t>(editorLinedefs.size() - 1)));
    editorVertices[vertexIdMap[ld.end]].connectedLineDefs.emplace_back(
      makeObjectId(EditorObjectType::LINEDEF, static_cast<uint32_t>(editorLinedefs.size() - 1)));
  }

  // TODO: process sectors

  this->level = std::make_unique<EditorLevel>(
    std::move(editorVertices), std::move(editorLinedefs), std::move(editorSectors), _level.sidedefs);
}

void EditorState::reset()
{
  for (auto &vertex : level->vertices) {
    vertex.selected = false;
    vertex.hovered = false;
  }

  for (auto &linedef : level->linedefs) {
    linedef.selected = false;
    linedef.hovered = false;
  }

  for (auto &sector : level->sectors) {
    sector.selected = false;
    sector.hovered = false;
  }

  selection.clear();

  isDragging = false;
  draggingOffset = { 0, 0 };
  draggingStart = { 0, 0 };

  isCreatingLine = false;
  lineStartVertexId = 0;

  renderOptionsWindow = false;
  optionsWindowPos = { 0, 0 };

  canvasOrigin = { 0.0f, 0.0f };
  canvasScroll = { 0.0f, 0.0f };
  canvasZoom = 1.0f;
}

void Editor::updateAABB(uint32_t sectorID)
{
  EditorSector &sec = this->state.get()->findSector(sectorID);

  // set values to max/min double values, so the first vertex overrides them
  sec.bounding_box.minX = std::numeric_limits<int16_t>::max();
  sec.bounding_box.minY = std::numeric_limits<int16_t>::max();
  sec.bounding_box.maxX = std::numeric_limits<int16_t>::lowest();
  sec.bounding_box.maxY = std::numeric_limits<int16_t>::lowest();

  for (int16_t ldId : sec.linedefIds) {
    const auto &ld = state.get()->findLinedef(ldId);

    const auto &start = state.get()->findVertex(ld.start);
    const auto &end = state.get()->findVertex(ld.end);

    // check start
    if (start.x < sec.bounding_box.minX) sec.bounding_box.minX = start.x;
    if (start.x > sec.bounding_box.maxX) sec.bounding_box.maxX = start.x;

    if (start.y < sec.bounding_box.minY) sec.bounding_box.minY = start.y;
    if (start.y > sec.bounding_box.maxY) sec.bounding_box.maxY = start.y;

    // check end
    if (end.x < sec.bounding_box.minX) sec.bounding_box.minX = end.x;
    if (end.x > sec.bounding_box.maxX) sec.bounding_box.maxX = end.x;

    if (end.y < sec.bounding_box.minY) sec.bounding_box.minY = end.y;
    if (end.y > sec.bounding_box.maxY) sec.bounding_box.maxY = end.y;
  }
}

Editor::Editor(gameloop::Level &_level, uint16_t _width, uint16_t _height)
{
  this->state = std::make_unique<EditorState>(_level, _width, _height);
  this->m_inputHandler = std::make_unique<EditorInputHandler>();
  this->m_history = std::make_unique<commands::CommandHistory>();
}

void Editor::processInput() { this->m_inputHandler->processInput(*this->state, *this->m_history); }

void Editor::addLineDef(int32_t sectorId, gameloop::LineDef &linedef)
{
  state.get()->level.get()->linedefs.emplace_back(linedef);

  if (sectorId == -1) return;

  auto sector = state.get()->findSector(sectorId);

  sector.linedefIds.emplace_back();
  this->updateAABB(sectorId);
}

void Editor::addLineDef(gameloop::Vertex start, gameloop::Vertex end)
{
  // level.vertices.emplace_back(start);
  // level.vertices.emplace_back(end);

  // gameloop::LineDef ld{ .start = static_cast<int16_t>(level.vertices.size() - 2),
  //   .end = static_cast<int16_t>(level.vertices.size() - 1),
  //   .type = gameloop::LineDefType::REGULAR,
  //   .frontSidedef = -1,
  //   .backSidedef = -1 };

  // level.linedefs.emplace_back(ld);
}

void Editor::addVertex(int32_t sectorId, int16_t x, int16_t y)
{
  m_history->execute(std::unique_ptr<commands::AddVertexCommand>(new commands::AddVertexCommand({ x, y })), *state);

  if (sectorId == -1) return;

  float_t dMin1 = std::numeric_limits<float_t>::max(), dMin2 = std::numeric_limits<float_t>::max();
  int16_t firstVertexIdx, secondVertexIdx;

  // find nearest two vertices with which to connect a vertex
  auto &sector = state->findSector(sectorId);

  for (int16_t ldIndex : sector.linedefIds) {
    auto &ld = state->findLinedef(ldIndex);

    auto &start = state->findVertex(ld.start);
    auto &end = state->findVertex(ld.end);

    float_t d1 = math_utils::getDistanceSq(start.x, start.y, x, y);
    float_t d2 = math_utils::getDistanceSq(end.x, end.y, x, y);

    if (d1 < dMin1) {
      dMin2 = dMin1;
      dMin1 = d1;
      secondVertexIdx = firstVertexIdx;
      firstVertexIdx = ld.start;
    } else if (d1 < dMin2) {
      dMin2 = d1;
      secondVertexIdx = ld.start;
    }

    if (d2 < dMin1) {
      dMin2 = dMin1;
      dMin1 = d2;
      secondVertexIdx = firstVertexIdx;
      firstVertexIdx = ld.end;
    } else if (d2 < dMin2) {
      dMin2 = d2;
      secondVertexIdx = ld.end;
    }
  }
}

void Editor::executeCommand(std::unique_ptr<commands::Command> cmd) { m_history->execute(std::move(cmd), *state); }

}// namespace editor