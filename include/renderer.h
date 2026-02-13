#pragma once

#include "gameloop.h"

#include <vector>

// Forward declaration
namespace framebuffer {
class FrameBuffer;
}

namespace renderer {

class Renderer
{
public:
  Renderer(framebuffer::FrameBuffer &fb, uint16_t canvasWidth, uint16_t canvasHeight);
  void render(const GameState &gameState, const Level &level);
  void resetClippingArrays();

private:
  void drawColumn(int32_t x, int32_t y0, int32_t y1, uint32_t color);
  void drawSolidWall(int x, int32_t &projectedCeilingZ, int32_t &projectedFloorZ);
  void drawCeiling(int32_t x, int32_t projectedCeilingY, uint32_t color);
  void drawFloor(int32_t x, int32_t projectedFloorY, uint32_t color);
  void drawDefaultPortal(int32_t x,
    int32_t projectedFloorY,
    int32_t projectedCeilingY,
    int32_t nextFloorY,
    int32_t nextCeilY);

  int32_t projectZ(double_t z, double_t inv_y) const;
  int32_t projectX(double_t x, double_t inv_y) const;

  std::vector<int32_t> floorClipping;
  std::vector<int32_t> ceilingClipping;

  framebuffer::FrameBuffer &fb;

  uint16_t canvasWidth;
  uint16_t canvasHeight;
};
}// namespace renderer
