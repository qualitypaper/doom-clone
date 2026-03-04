#include "editor.h"

#include "commands.h"
#include "editor_input_handler.h"
#include "gameloop.h"
#include "math_utils.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

AABB::AABB(const Vertex start, const Vertex end)
{
  this->maxX = std::max(start.x, end.x);
  this->minX = std::min(start.x, end.x);
  this->maxY = std::max(start.y, end.y);
  this->minY = std::min(start.y, end.y);
}

bool EditorVertex::isAnyConnectedLineDefSelected(const EditorState &state) const
{
  return std::ranges::any_of(
    connectedLineDefs, [&](auto ldObjectId) { return state.findLinedef(getObjectIndex(ldObjectId)).selected; });
}

void EditorLineDef::remove(const EditorState &state, const uint32_t ldId)
{
  const uint32_t lastLdIndex = static_cast<uint32_t>(state.level->linedefs.size() - 1);
  auto &ld = state.level->linedefs[ldId];
  auto &lastLd = state.level->linedefs.back();
  auto &vStart = state.findVertex(ld.start);
  auto &vEnd = state.findVertex(ld.end);
  auto &lastLdStart = state.findVertex(lastLd.start);
  auto &lastLdEnd = state.findVertex(lastLd.end);

  if (ldId != lastLdIndex) {
    // Update indices in connected vertices
    for (auto &ldObjectId : lastLdStart.connectedLineDefs) {
      if (getObjectIndex(ldObjectId) == lastLdIndex) {
        ldObjectId = makeObjectId(EditorObjectType::LINEDEF, ldId);
        break;
      }
    }
    for (auto &ldObjectId : lastLdEnd.connectedLineDefs) {
      if (getObjectIndex(ldObjectId) == lastLdIndex) {
        ldObjectId = makeObjectId(EditorObjectType::LINEDEF, ldId);
        break;
      }
    }

    ld = lastLd;
  }
  std::erase_if(
    vStart.connectedLineDefs, [ldId](const auto &ldObjectId) { return getObjectIndex(ldObjectId) == ldId; });
  std::erase_if(vEnd.connectedLineDefs, [ldId](const auto &ldObjectId) { return getObjectIndex(ldObjectId) == ldId; });

  state.level->linedefs.pop_back();
}

void EditorVertex::remove(EditorState &state, const uint32_t vertexId)
{
  auto &vertex = state.level->vertices[vertexId];
  // Remove all linedefs that reference this vertex (iterate backwards to avoid index issues)
  for (int i = static_cast<int>(vertex.connectedLineDefs.size()) - 1; i >= 0; i--) {
    uint32_t ldId = getObjectIndex(vertex.connectedLineDefs[i]);
    const auto &ld = state.level->linedefs[ldId];

    if (ld.start == vertexId) {
      auto &vEnd = state.findVertex(ld.end);

      std::erase_if(
        vEnd.connectedLineDefs, [&ldId](const auto &ldObjectId) { return getObjectIndex(ldObjectId) == ldId; });
    } else if (ld.end == vertexId) {
      auto &vStart = state.findVertex(ld.start);

      std::erase_if(
        vStart.connectedLineDefs, [&ldId](const auto &ldObjectId) { return getObjectIndex(ldObjectId) == ldId; });
    }

    EditorLineDef::remove(state, ldId);
  }

  // Remove the vertex using swap-and-pop
  const auto lastVertexIndex = static_cast<uint32_t>(state.level->vertices.size() - 1);

  if (vertexId != lastVertexIndex) {
    const auto &lastVertex = state.level->vertices.back();

    for (const uint32_t ldObjectId : lastVertex.connectedLineDefs) {
      const auto ldIndex = getObjectIndex(ldObjectId);
      auto &ld = state.findLinedef(ldIndex);

      if (ld.start == lastVertexIndex) ld.start = vertexId;
      if (ld.end == lastVertexIndex) ld.end = vertexId;
    }

    vertex = lastVertex;
  }

  state.selection.clear();
  state.level->vertices.pop_back();
}

EditorState::EditorState(Level &_level, const uint16_t _width, const uint16_t _height) : width(_width), height(_height)
{
  std::vector<EditorVertex> editorVertices;
  std::vector<EditorLineDef> editorLinedefs;
  std::vector<EditorSector> editorSectors;
  std::vector<EditorSidedef> editorSidedefs;

  editorVertices.reserve(_level.vertices.size());
  editorLinedefs.reserve(_level.linedefs.size());
  editorSectors.reserve(_level.sectors.size());
  editorSidedefs.reserve(_level.sidedefs.size());

  std::unordered_map<size_t, uint32_t> vertexIdMap;
  vertexIdMap.reserve(_level.vertices.size());

  // process vertices
  for (size_t i = 0; i < _level.vertices.size(); ++i) {
    const auto &v = _level.vertices[i];

    auto convertedImvec2 = math_utils::fromCenterCoordinates(math_utils::convertVertexIntoImVec2(v), _width, _height);
    editorVertices.emplace_back(convertedImvec2.x, convertedImvec2.y);
    vertexIdMap[i] = makeObjectId(EditorObjectType::VERTEX, editorVertices.size() - 1);
  }

  // process linedefs
  for (const auto &[start, end, type, frontSidedef, backSidedef] : _level.linedefs) {
    editorLinedefs.emplace_back(vertexIdMap[start], vertexIdMap[end], type, frontSidedef, backSidedef);

    editorVertices[start].connectedLineDefs.emplace_back(
      makeObjectId(EditorObjectType::LINEDEF, static_cast<uint32_t>(editorLinedefs.size() - 1)));
    editorVertices[end].connectedLineDefs.emplace_back(
      makeObjectId(EditorObjectType::LINEDEF, static_cast<uint32_t>(editorLinedefs.size() - 1)));
  }

  // process sectors
  for (const auto &[floorHeight, ceilingHeight, specialType, lightLevel, tag, color] : _level.sectors) {
    editorSectors.emplace_back(floorHeight, ceilingHeight, specialType, lightLevel, tag, color);
  }

  // process sidedefs
  for (const auto &sd : _level.sidedefs) { editorSidedefs.emplace_back(sd.sectorId, sd.xOffset, sd.yOffset); }


  this->level = std::make_unique<EditorLevel>(
    std::move(editorVertices), std::move(editorLinedefs), std::move(editorSectors), std::move(editorSidedefs));
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

  isBlockSelecting = false;
  blockSelectionStart = { 0, 0 };

  renderOptionsWindow = false;
  optionsWindowPos = { 0, 0 };

  canvasOrigin = { 0.0f, 0.0f };
  canvasScroll = { 0.0f, 0.0f };
  canvasZoom = 1.0f;
}

void Editor::updateAABB(const uint32_t sectorID) const
{
  EditorSector &sec = state->findSector(sectorID);

  // set values to max/min double values, so the first vertex overrides them
  sec.bounding_box.minX = std::numeric_limits<int16_t>::max();
  sec.bounding_box.minY = std::numeric_limits<int16_t>::max();
  sec.bounding_box.maxX = std::numeric_limits<int16_t>::lowest();
  sec.bounding_box.maxY = std::numeric_limits<int16_t>::lowest();

  for (const uint16_t ldId : sec.linedefIds) {
    const auto &ld = state->findLinedef(ldId);

    const auto &start = state->findVertex(ld.start);
    const auto &end = state->findVertex(ld.end);

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

Editor::Editor(Level &_level, uint16_t _width, uint16_t _height)
  : m_inputHandler(std::make_unique<EditorInputHandler>()), m_history(std::make_unique<CommandHistory>()),
    state(std::make_unique<EditorState>(_level, _width, _height))
{}

void Editor::processInput(const float_t vertexRadius) const
{ EditorInputHandler::processInput(*this->state, *this->m_history, vertexRadius); }

void Editor::addLineDef(const int32_t sectorId, LineDef &linedef) const
{
  state->level->linedefs.emplace_back(linedef);

  if (sectorId == -1) return;

  auto sector = state->findSector(sectorId);

  sector.linedefIds.emplace_back();
  this->updateAABB(sectorId);
}

void Editor::addVertex(const int16_t x, const int16_t y) const
{ m_history->execute(std::make_unique<AddVertexCommand>(EditorVertex(x, y)), *state); }

void Editor::executeCommand(std::unique_ptr<Command> cmd) const { m_history->execute(std::move(cmd), *state); }


void Editor::drawConnectedLine(const uint32_t vertexIndex) const
{
  state->isCreatingLine = true;
  state->lineStartVertexId = vertexIndex;
}
