#include "editor.h"

#include "commands.h"
#include "core/gameloop.h"
#include "core/math_utils.h"
#include "core/serialization/texture_serializer.h"
#include "core/serialization/wad_serializer.h"
#include "editor_input_handler.h"
#include "editor_renderer.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <ranges>
#include <vector>

AABB::AABB(const Vertex start, const Vertex end)
{
  this->maxX = std::max(start.x, end.x);
  this->minX = std::min(start.x, end.x);
  this->maxY = std::max(start.y, end.y);
  this->minY = std::min(start.y, end.y);
}

void DraggableObject::drag(uint32_t objectId, EditorState *state, CommandHistory *history)
{
  this->dragged = true;
  state->dragged.emplace(objectId);
}

void DraggableObject::resetDraggableState(EditorState *state)
{
  for (const uint32_t id : state->dragged) {
    EditorObject *object = state->findObject(id);
    object->dragged = false;
  }

  state->dragged.clear();
}

bool EditorVertex::isAnyConnectedLineDefSelected(const EditorState &state) const
{
  return std::ranges::any_of(connectedLineDefs,
                             [&](auto ldObjectId) { return state.findLinedef(getObjectIndex(ldObjectId)).selected; });
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
  std::erase_if(vStart.connectedLineDefs,
                [ldId](const auto &ldObjectId) { return getObjectIndex(ldObjectId) == ldId; });
  std::erase_if(vEnd.connectedLineDefs, [ldId](const auto &ldObjectId) { return getObjectIndex(ldObjectId) == ldId; });

  state.level->linedefs.pop_back();
}

void EditorVertex::remove(EditorState &state, const uint32_t vertexId)
{
  auto &vertex = state.findVertex(vertexId);
  const uint32_t vertexIndex = getObjectIndex(vertexId);

  // Remove all linedefs that reference this vertex (iterate backwards to avoid index issues)
  for (int i = static_cast<int>(vertex.connectedLineDefs.size()) - 1; i >= 0; i--) {
    uint32_t ldId = getObjectIndex(vertex.connectedLineDefs[i]);
    const auto &ld = state.level->linedefs[ldId];

    if (ld.start == vertexId) {
      auto &vEnd = state.findVertex(ld.end);

      std::erase_if(vEnd.connectedLineDefs,
                    [&ldId](const auto &ldObjectId) { return getObjectIndex(ldObjectId) == ldId; });
    } else if (ld.end == vertexId) {
      auto &vStart = state.findVertex(ld.start);

      std::erase_if(vStart.connectedLineDefs,
                    [&ldId](const auto &ldObjectId) { return getObjectIndex(ldObjectId) == ldId; });
    }

    EditorLineDef::remove(state, ldId);
  }

  // Remove the vertex using swap-and-pop
  const uint32_t lastVertexIndex = static_cast<uint32_t>(state.level->vertices.size() - 1);
  const uint32_t lastVertexId = makeObjectId(EditorObjectType::VERTEX, lastVertexIndex);

  if (vertexIndex != lastVertexIndex) {
    const auto &lastVertex = state.level->vertices.back();

    for (const uint32_t ldObjectId : lastVertex.connectedLineDefs) {
      auto &ld = state.findLinedef(ldObjectId);

      if (ld.start == lastVertexId)
        ld.start = vertexId;
      if (ld.end == lastVertexId)
        ld.end = vertexId;
    }

    vertex = lastVertex;
  }

  state.selection.clear();
  state.level->vertices.pop_back();
}

EditorState::EditorState(Level &_level,
                         const uint16_t _levelNum,
                         const uint16_t _numOfLevels,
                         const uint16_t _width,
                         const uint16_t _height)
  : width(_width), height(_height), numOfLevels(_numOfLevels), textures(nullptr)
{
  std::vector<EditorVertex> editorVertices;
  std::vector<EditorLineDef> editorLinedefs;
  std::vector<EditorSector> editorSectors;
  std::vector<EditorSidedef> editorSidedefs;

  editorVertices.reserve(_level.vertices.size());
  transformedVertices.resize(_level.vertices.size());

  editorLinedefs.reserve(_level.linedefs.size());
  editorSectors.reserve(_level.sectors.size());
  editorSidedefs.reserve(_level.sidedefs.size());

  this->level = std::make_unique<EditorLevel>(_level, _levelNum, _width, _height);
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
    if (start.x < sec.bounding_box.minX)
      sec.bounding_box.minX = start.x;
    if (start.x > sec.bounding_box.maxX)
      sec.bounding_box.maxX = start.x;

    if (start.y < sec.bounding_box.minY)
      sec.bounding_box.minY = start.y;
    if (start.y > sec.bounding_box.maxY)
      sec.bounding_box.maxY = start.y;

    // check end
    if (end.x < sec.bounding_box.minX)
      sec.bounding_box.minX = end.x;
    if (end.x > sec.bounding_box.maxX)
      sec.bounding_box.maxX = end.x;

    if (end.y < sec.bounding_box.minY)
      sec.bounding_box.minY = end.y;
    if (end.y > sec.bounding_box.maxY)
      sec.bounding_box.maxY = end.y;
  }
}

Editor::Editor(std::shared_ptr<SdlWindow> sdlWindow,
               Level &_level,
               const uint16_t _levelNum,
               const uint16_t _numOfLevels,
               std::vector<Texture> &textures)
  : m_history(std::make_shared<CommandHistory>()),
    state(std::make_shared<EditorState>(
      _level, _levelNum, _numOfLevels, sdlWindow->GetWidth(), sdlWindow->GetHeight()))
{
  this->m_inputHandler = std::make_unique<EditorInputHandler>(state, m_history);
  this->m_renderer = std::make_unique<EditorRenderer>(std::move(sdlWindow), *this);
  state->textures = &textures;
}

Editor::~Editor() = default;

void Editor::Render() const
{
  resetStateFrame();

  // Keep input/update orchestration in Editor; renderer only draws UI.
  const float vertexRadius = 4.0f * state->canvasZoom;
  ProcessInput(vertexRadius);
  TransformVertices();

  m_renderer->Render();
}

std::array<char8_t, 8> Editor::GetLevelName() const { return state->level->name; }

/**
 * function resets the state, the must be set to default on each frame
 */
void Editor::resetStateFrame() const
{
  DraggableObject::resetDraggableState(state.get());

  if (state->hoveredObjectId != UINT32_MAX) {
    EditorObject *object = state->findObject(state->hoveredObjectId);
    object->hovered = false;

    state->hoveredObjectId = UINT32_MAX;
  }
}

void Editor::ProcessInput(const float_t vertexRadius) const { m_inputHandler->ProcessInput(vertexRadius); }

void Editor::addLineDef(const int32_t sectorId, LineDef &linedef) const
{
  state->level->linedefs.emplace_back(linedef);

  if (sectorId == -1)
    return;

  EditorSector &sector = state->findSector(sectorId);

  sector.linedefIds.emplace_back();
  this->updateAABB(sectorId);
}

void Editor::addVertex(const double x, const double y) const
{
  const EditorVertex temp{ x, y };
  // const ImVec2 newVertex = transformVertex(temp);

  m_history->execute(std::make_unique<AddVertexCommand>(EditorVertex(temp.x, temp.y)), *state);
}

void Editor::executeCommand(std::unique_ptr<Command> cmd) const { m_history->execute(std::move(cmd), *state); }


void Editor::drawConnectedLine(const uint32_t vertexId) const
{
  state->isCreatingLine = true;
  state->lineStartVertexId = vertexId;
}

void EditorVertex::drag(const uint32_t objectId, EditorState *state, CommandHistory *history)
{
  assert(getObjectType(objectId) == EditorObjectType::VERTEX);

  if (dragged)
    return;

  DraggableObject::drag(objectId, state, history);

  ImVec2 unzoomedOffset = Editor::unscale(state->draggingOffset, state->canvasZoom);

  auto cmd = std::make_unique<MoveVertexCommand>(objectId, unzoomedOffset);

  history->execute(std::move(cmd), *state);
}

void EditorLineDef::drag(const uint32_t objectId, EditorState *state, CommandHistory *history)
{
  assert(getObjectType(objectId) == EditorObjectType::LINEDEF);

  if (dragged)
    return;

  DraggableObject::drag(objectId, state, history);

  auto &startVertex = state->findVertex(start), &endVertex = state->findVertex(end);

  startVertex.drag(start, state, history);
  endVertex.drag(end, state, history);
}

ImVec2 Editor::TransformVertex(const EditorVertex &v) const
{
  ImVec2 transformed = zoomVertex(v);

  // apply dragging
  if (v.selected || v.isAnyConnectedLineDefSelected(*state)) {
    transformed.x = transformed.x + state->draggingOffset.x;
    transformed.y = transformed.y + state->draggingOffset.y;
  }

  // apply scrolling
  transformed.x = transformed.x + state->scrollingOffset.x;
  transformed.y = transformed.y + state->scrollingOffset.y;

  return transformed;
}

ImVec2 Editor::UntransformVertex(ImVec2 transformed) const
{
  // apply scrolling
  transformed.x = transformed.x - state->scrollingOffset.x;
  transformed.y = transformed.y - state->scrollingOffset.y;

  return zoomVertex(transformed, 1 / state->canvasZoom);
}

/**
 * applies transformations to all vertices like zoom, drag, scroll
 * only if one of this parameters change the method will recalculate the vector
 */
void Editor::TransformVertices() const
{
  // static float prevZoom = -1;
  // static size_t undoStackSize = 0;
  // static ImVec2 draggingOffset = { 0, 0 }, scrollingOffset = { 0, 0 };
  //
  // TODO: trigger transformVertices only when one of these parameters change, currently it is called on each frame, but
  // it should be optimized

  // prevZoom = state->canvasZoom;
  // draggingOffset = state->draggingOffset;
  // scrollingOffset = state->scrollingOffset;
  // undoStackSize = m_history->undoStack.size();

  const std::vector<EditorVertex> &vertices = state->level->vertices;

  if (state->transformedVertices.size() != vertices.size()) {
    state->transformedVertices.resize(vertices.size());
  }

  // editor vertex contains a vector, so copy is unacceptable
  for (auto [i, v] : std::ranges::views::enumerate(vertices)) {
    state->transformedVertices[i] = TransformVertex(v);
  }
}

void Editor::addEmptyLevel() const
{
  state->level->Save(state->level->name, state->numOfLevels, this->state->width, this->state->height);
  state->numOfLevels++;
  const std::array<char8_t, 8> levelName = WadSerializer::MakeLevelName(state->numOfLevels);

  state->level = std::make_unique<EditorLevel>(state->numOfLevels, levelName);
}

void Editor::changeLevel(const uint16_t newLevelNum) const
{
  if (state->level->levelNum == newLevelNum)
    return;

  state->level->Save(state->level->name, state->numOfLevels, this->state->width, this->state->height);

  state->reset();

  auto levelName = std::array<char8_t, 8>{};
  const std::string nameStr = std::format("Map{}", newLevelNum);

  const size_t copyLen = std::min(nameStr.size(), size_t{ 8 });
  std::memcpy(levelName.data(), nameStr.data(), copyLen);

  auto newLevel = std::make_unique<Level>(levelName);

  EditorLevel::Load(newLevel);
  state->level = std::make_unique<EditorLevel>(*newLevel, newLevelNum, this->state->width, this->state->height);
}


EditorLevel::EditorLevel(const Level &_level, const uint16_t _levelNum, const uint16_t _width, const uint16_t _height)
  : levelNum(_levelNum), name(_level.name)
{
  vertices.reserve(_level.vertices.size());
  sidedefs.reserve(_level.sidedefs.size());
  linedefs.reserve(_level.linedefs.size());
  sectors.reserve(_level.sectors.size());

  // process vertices
  for (auto &vertice : _level.vertices) {
    const Vertex *v = &vertice;

    const Vertex centered = v->fromCenterCoords(_width, _height);
    vertices.emplace_back(centered);
  }

  // process sectors
  for (const auto &sector : _level.sectors) {
    sectors.emplace_back(
      sector.floorHeight, sector.ceilingHeight, sector.specialType, sector.lightLevel, sector.tag, sector.color);
  }

  // process sidedefs
  for (const auto &sd : _level.sidedefs) {
    sidedefs.emplace_back(sd.sector ? sd.sector - _level.sectors.data() : -1, sd.xOffset, sd.yOffset);
  }

  // process linedefs
  for (const auto &[start, end, type, frontSidedef, backSidedef] : _level.linedefs) {
    const size_t startIdx = start - _level.vertices.data();
    const size_t endIdx = end - _level.vertices.data();

    linedefs.emplace_back(makeObjectId(EditorObjectType::VERTEX, startIdx),
                          makeObjectId(EditorObjectType::VERTEX, endIdx),
                          type,
                          !frontSidedef ? -1 : frontSidedef - _level.sidedefs.data(),
                          !backSidedef ? -1 : backSidedef - _level.sidedefs.data());


    vertices[startIdx].connectedLineDefs.emplace_back(
      makeObjectId(EditorObjectType::LINEDEF, static_cast<uint32_t>(linedefs.size() - 1)));
    vertices[endIdx].connectedLineDefs.emplace_back(
      makeObjectId(EditorObjectType::LINEDEF, static_cast<uint32_t>(linedefs.size() - 1)));
  }
}