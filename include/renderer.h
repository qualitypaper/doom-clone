#pragma once

#include "bsp.h"
#include "gameloop.h"
#include "math_utils.h"

#include <vector>

static constexpr int16_t NF_SUBSECTOR = 0x8000;
static constexpr uint16_t MAX_VISPLANES = 128;
static constexpr uint16_t MAX_SEGMENTS = 64;

// Forward declaration
class FrameBuffer;

class Renderer
{
public:
  Renderer(FrameBuffer &fb, std::shared_ptr<Level> _level, uint16_t canvasWidth, uint16_t canvasHeight);
  void Render(const GameState &gameState);
  bool ClipSolidWall(int16_t start, int16_t end, const Seg &seg, const SideDef &sidedef, const Player &player);
  void ResetClippingArrays();

private:
  void DrawColumn(int32_t x, int32_t y0, int32_t y1, uint32_t color) const;
  void DrawSolidWall(int32_t x, int32_t projectedCeilingZ, int32_t projectedFloorZ, uint32_t color);
  void DrawCeiling(int32_t x, int32_t projectedCeilingY, uint32_t color);
  void DrawFloor(int32_t x, int32_t projectedFloorY, uint32_t color);
  void DrawDefaultPortal(int32_t x,
    int32_t projectedFloorY,
    int32_t projectedCeilingY,
    int32_t nextFloorY,
    int32_t nextCeilY);
  Visplane *FindVisPlane(int16_t height, uint32_t color, int16_t lightLevel);

  void RenderVisPlanes();
  void StoreWallRange(const ClipRange &range, const Seg &seg, const SideDef &sd, const Player &player);
  void RenderSegment(const Seg &seg, const Player &player);
  void CheckVisPlane();
  void RenderSSector(const Player &player, const SubSector &subsector);
  void RenderBSPNode(const GameState &gameState, int16_t nodeIndex);

  template<HasXY T> static bool PointOnSide(T v, const BspNode &node)
  {
    // Calculate vector from the partition line's origin to the player
    const double_t dx = v.x - node.x;
    const double_t dy = v.y - node.y;

    // 2D Cross Product
    const double_t leftSide = (node.dx * dy) - (node.dy * dx);

    return leftSide > 0;
  }

  [[nodiscard]] int16_t ProjectZ(double_t z, double_t inv_y) const;
  [[nodiscard]] int16_t ProjectX(double_t x, double_t inv_y) const;

private:
  std::vector<int32_t> m_floorClipping;
  std::vector<int32_t> m_ceilingClipping;
  std::vector<ClipRange> m_solidsegs;
  std::vector<Visplane> m_visplanes;

  Visplane *m_ceilplane = nullptr, *m_floorplane = nullptr;

  FrameBuffer &m_fb;
  std::shared_ptr<Level> m_level;

  uint16_t m_canvasWidth;
  uint16_t m_canvasHeight;
};
