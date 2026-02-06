#include "editor.h"
#include "gameloop.h"
#include "math_utils.h"

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

EditorState::EditorState(gameloop::Level &_level, uint16_t _width, uint16_t _height)
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
    uint32_t id = getNextId();

    auto convertedImvec2 = math_utils::fromCenterCoordinates(math_utils::convertVertexIntoImVec2(v), _width, _height);
    editorVertices.emplace_back(id, convertedImvec2.x, convertedImvec2.y);
    vertexIdMap[i] = id;
  }

  // process linedefs
  for (auto &ld : _level.linedefs) {
    editorLinedefs.emplace_back(
      getNextId(), vertexIdMap[ld.start], vertexIdMap[ld.end], ld.type, ld.frontSidedef, ld.backSidedef);
  }

  // TODO: process sectors

  this->level = std::make_unique<EditorLevel>(
    std::move(editorVertices), std::move(editorLinedefs), std::move(editorSectors), _level.sidedefs);
}

void Editor::updateAABB(uint32_t sectorID)
{
  EditorSector *sec = this->state.get()->findSector(sectorID);

  // set values to max/min double values, so the first vertex overrides them
  sec->bounding_box.minX = std::numeric_limits<int16_t>::max();
  sec->bounding_box.minY = std::numeric_limits<int16_t>::max();
  sec->bounding_box.maxX = std::numeric_limits<int16_t>::lowest();
  sec->bounding_box.maxY = std::numeric_limits<int16_t>::lowest();

  for (int16_t ldId : sec->linedefIds) {
    const auto &ld = state.get()->findLinedef(ldId);
    if (!ld) continue;

    const auto &start = state.get()->findVertex(ld->start);
    const auto &end = state.get()->findVertex(ld->end);
    if (!start || !end) continue;

    // check start
    if (start->x < sec->bounding_box.minX) sec->bounding_box.minX = start->x;
    if (start->x > sec->bounding_box.maxX) sec->bounding_box.maxX = start->x;

    if (start->y < sec->bounding_box.minY) sec->bounding_box.minY = start->y;
    if (start->y > sec->bounding_box.maxY) sec->bounding_box.maxY = start->y;

    // check end
    if (end->x < sec->bounding_box.minX) sec->bounding_box.minX = end->x;
    if (end->x > sec->bounding_box.maxX) sec->bounding_box.maxX = end->x;

    if (end->y < sec->bounding_box.minY) sec->bounding_box.minY = end->y;
    if (end->y > sec->bounding_box.maxY) sec->bounding_box.maxY = end->y;
  }
}

Editor::Editor(gameloop::Level &_level, uint16_t _width, uint16_t _height) { this->state = std::make_unique<EditorState>(_level, _width, _height); }

void Editor::addLineDef(uint32_t sectorId, gameloop::LineDef &linedef)
{
  // state.get()->level.get()->linedefs.emplace_back(state->getNextId(), linedef);

  if (sectorId == 0) return;

  auto sector = state.get()->findSector(sectorId);
  if (!sector) return;

  sector->linedefIds.emplace_back(state->nextId - 1);
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

void Editor::addVertex(uint32_t sectorId, gameloop::Vertex &vertex)
{
  state->level->vertices.emplace_back(state->getNextId(), vertex.x, vertex.y);

  if (sectorId == 0) return;

  float_t dMin1 = std::numeric_limits<float_t>::max(), dMin2 = std::numeric_limits<float_t>::max();
  int16_t firstVertexIdx, secondVertexIdx;

  // find nearest two vertices with which to connect a vertex
  auto sector = state->findSector(sectorId);
  if (!sector) return;

  for (int16_t ldIndex : sector->linedefIds) {
    auto ld = state->findLinedef(ldIndex);
    if (!ld) continue;

    auto start = state->findVertex(ld->start);
    auto end = state->findVertex(ld->end);

    float_t d1 = math_utils::getDistanceSq(start->x, start->y, vertex.x, vertex.y);
    float_t d2 = math_utils::getDistanceSq(end->x, end->y, vertex.x, vertex.y);

    if (d1 < dMin1) {
      dMin2 = dMin1;
      dMin1 = d1;
      secondVertexIdx = firstVertexIdx;
      firstVertexIdx = ld->start;
    } else if (d1 < dMin2) {
      dMin2 = d1;
      secondVertexIdx = ld->start;
    }

    if (d2 < dMin1) {
      dMin2 = dMin1;
      dMin1 = d2;
      secondVertexIdx = firstVertexIdx;
      firstVertexIdx = ld->end;
    } else if (d2 < dMin2) {
      dMin2 = d2;
      secondVertexIdx = ld->end;
    }
  }
}

}// namespace editor
