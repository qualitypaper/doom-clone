#pragma once
#include "gameloop.h"

#include <array>

struct EditorSeg
{
  gameloop::Vertex *start;
  gameloop::Vertex *end;
  gameloop::LineDef *linedef;
  int8_t side;// 0 for front, 1 for back
};

struct BspNode
{
  int x, y, dx, dy;
  std::array<int16_t, 4> leftBoundingBox, rightBoundingBox;
  int16_t leftChild, rightChild;
};
