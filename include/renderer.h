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
  void render(const gameloop::GameState &gameState, const gameloop::Level &level);
  void resetClippingArrays();

private:
  void drawColumn(int16_t x, int16_t y0, int16_t y1, uint32_t color);
  void drawSolidWall(int x, int32_t &projectedCeilingZ, int32_t &projectedFloorZ);
  void drawCeiling(int32_t x, int32_t projectedCeilingY);
  void drawFloor(int32_t x, int32_t projectedFloorY);
  void drawDefaultPortal(int32_t x,
    int32_t projectedFloorY,
    int32_t projectedCeilingY,
    int32_t nextFloorY,
    int32_t nextCeilY);

  int32_t projectZ(int32_t z, double_t inv_y);
  int32_t projectX(int32_t x, double_t inv_y);

  std::vector<int32_t> floorClipping;
  std::vector<int32_t> ceilingClipping;

  framebuffer::FrameBuffer &fb;

  uint16_t canvasWidth;
  uint16_t canvasHeight;
};
}// namespace renderer
