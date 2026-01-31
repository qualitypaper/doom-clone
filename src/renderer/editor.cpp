#include "editor.h"
#include "gameloop.h"
#include "math_utils.h"
#include <limits>
#include <vector>

namespace editor {

AABB::AABB(gameloop::Vertex start, gameloop::Vertex end)
{
  this->maxX = std::max(start.x, end.x);
  this->minX = std::min(start.x, end.x);
  this->maxY = std::max(start.y, end.y);
  this->minY = std::min(start.y, end.y);
}

void Editor::updateAABB(int16_t sectorID)
{
  EditorSector &sec = this->editorSectors[sectorID];

  // set values to max/min double values, so the first vertex overrides them
  sec.boundingBox.minX = std::numeric_limits<int16_t>::max();
  sec.boundingBox.minY = std::numeric_limits<int16_t>::max();
  sec.boundingBox.maxX = std::numeric_limits<int16_t>::lowest();
  sec.boundingBox.maxY = std::numeric_limits<int16_t>::lowest();

  for (int16_t ldIndex : sec.linedefsIndices) {
    auto &ld = level.linedefs[ldIndex];
    auto &start = level.vertices[ld.start];
    auto &end = level.vertices[ld.end];

    // check start
    if (start.x < sec.boundingBox.minX) sec.boundingBox.minX = start.x;
    if (start.x > sec.boundingBox.maxY) sec.boundingBox.maxX = start.x;

    if (start.y < sec.boundingBox.minY) sec.boundingBox.minY = start.y;
    if (start.y > sec.boundingBox.maxY) sec.boundingBox.maxY = start.y;

    // check end
    if (end.x < sec.boundingBox.minX) sec.boundingBox.minX = end.x;
    if (end.x > sec.boundingBox.maxY) sec.boundingBox.maxX = end.x;

    if (end.y < sec.boundingBox.minY) sec.boundingBox.minY = end.y;
    if (end.y > sec.boundingBox.maxY) sec.boundingBox.maxY = end.y;
  }
}

void Editor::addToSector(gameloop::Level &level, uint16_t i, gameloop::LineDef &linedef, gameloop::SideDef &sidedef)
{
  auto &sector = this->editorSectors[sidedef.sectorId];

  if (sector.linedefsIndices.size() > 0) {
    sector.linedefsIndices.push_back(i);
    this->updateAABB(sidedef.sectorId);
  } else {
    AABB bounds(level.vertices[linedef.start], level.vertices[linedef.end]);

    std::vector<int16_t> linedefindices;
    linedefindices.push_back(i);

    sector = { bounds, linedefindices };
  }
}

Editor::Editor(gameloop::Level &level) : level(level)
{
  this->editorSectors.reserve(level.sectors.size());

  for (uint16_t i = 0; i < level.linedefs.size(); i++) {
    auto &ld = level.linedefs[i];
    auto &frontSidedef = level.sidedefs[ld.frontSidedef];
    addToSector(level, i, ld, frontSidedef);

    if (ld.backSidedef != -1) {
      auto &backSidedef = level.sidedefs[ld.backSidedef];
      addToSector(level, i, ld, backSidedef);
    }
  }
}

void Editor::addSector(const AABB &bounds, const std::vector<int16_t> &linedefsIndices)
{
  this->editorSectors.push_back({ bounds, linedefsIndices });
}

void Editor::addLineDef(int16_t sectorIndex, const gameloop::LineDef &linedef)
{
  level.linedefs.push_back(linedef);
  if (sectorIndex == -1) return;

  auto &sec = this->editorSectors[sectorIndex];

  sec.linedefsIndices.push_back(level.linedefs.size() - 1);
  this->updateAABB(sectorIndex);
}

void Editor::addLineDef(gameloop::Vertex start, gameloop::Vertex end)
{
  level.vertices.push_back(start);
  level.vertices.push_back(end);

  gameloop::LineDef ld{ .start = static_cast<int16_t>(level.vertices.size() - 2),
    .end = static_cast<int16_t>(level.vertices.size() - 1),
    .type = gameloop::LineDefType::REGULAR,
    .frontSidedef = -1,
    .backSidedef = -1 };

  level.linedefs.push_back(ld);
}

void Editor::addVertex(int16_t sectorIndex, const gameloop::Vertex &vertex)
{
  level.vertices.push_back(vertex);

  if (sectorIndex == -1) return;

  float_t d1 = 0, d2 = 0;
  gameloop::Vertex v1, v2;

  // find nearest two vertices with which to connect a vertex
  for (int16_t ldIndex : this->editorSectors[sectorIndex].linedefsIndices) {
    auto &ld = level.linedefs[ldIndex];


    if () {}
  }

}// namespace editor
