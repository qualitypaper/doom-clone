#pragma once

#include "bsp.h"
#include "gameloop.h"

#include <vector>

// Forward declaration
class FrameBuffer;

class Renderer
{
public:
  Renderer(FrameBuffer &fb, std::shared_ptr<Level> _level, uint16_t canvasWidth, uint16_t canvasHeight);
  void Render(const GameState &gameState);
  void RenderSegment(const Seg &seg, const GameState &gameState);
  void RenderBSPNode(const GameState &gameState, int16_t nodeIndex);
  void ResetClippingArrays();

private:
  void DrawColumn(int32_t x, int32_t y0, int32_t y1, uint32_t color) const;
  void DrawSolidWall(int32_t x, int32_t projectedCeilingZ, int32_t projectedFloorZ);
  void DrawCeiling(int32_t x, int32_t projectedCeilingY, uint32_t color);
  void DrawFloor(int32_t x, int32_t projectedFloorY, uint32_t color);
  void DrawDefaultPortal(int32_t x,
    int32_t projectedFloorY,
    int32_t projectedCeilingY,
    int32_t nextFloorY,
    int32_t nextCeilY);

  static bool PointOnSide(Vertex v, const BspNode &node);

  [[nodiscard]] int32_t ProjectZ(double_t z, double_t inv_y) const;
  [[nodiscard]] int32_t ProjectX(double_t x, double_t inv_y) const;

private:
  std::vector<int32_t> m_floorClipping;
  std::vector<int32_t> m_ceilingClipping;
  std::vector<bool> m_solidSegs;


  FrameBuffer &m_fb;
  std::shared_ptr<Level> m_level;

  uint16_t m_canvasWidth;
  uint16_t m_canvasHeight;
};
