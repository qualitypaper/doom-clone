#pragma once

#include <cstdint>
#include <vector>

#include "gameloop.h"

namespace editor {
struct AABB
{
  AABB() = default;
  AABB(gameloop::Vertex start, gameloop::Vertex end);
  int16_t maxX, maxY;
  int16_t minX, minY;

  bool contains(int16_t x, int16_t y) { return x >= minX && x <= maxX && y >= minY && y <= maxY; }
};

struct EditorSector
{
  AABB boundingBox;
  std::vector<int16_t> linedefsIndices;
};

class Editor
{
private:
  std::vector<EditorSector> editorSectors;
  gameloop::Level &level;

  void addToSector(gameloop::Level &level, uint16_t i, gameloop::LineDef &linedef, gameloop::SideDef &sidedef);
  void updateAABB(int16_t sectorIndex);

public:
  Editor(gameloop::Level &level);
  void addSector(const AABB &bounds, const std::vector<int16_t> &linedefsIndices);
  void addLineDef(gameloop::Vertex start, gameloop::Vertex end);
  void addLineDef(int16_t sectorIndex, const gameloop::LineDef &lineDef);
  void addVertex(int16_t sectorIndex, const gameloop::Vertex &vertex);
};
}// namespace editor
