#pragma once

#include "bsp.h"
#include "gameloop.h"
#include "math_utils.h"
#include "tables.h"

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
  void ResetClippingArrays();

private:
  void DrawColumn(int32_t x, int32_t y0, int32_t y1, uint32_t color) const;
  void DrawSolidWall(int32_t x, int32_t projectedCeilingZ, int32_t projectedFloorZ, uint32_t color);
  void DrawDefaultPortal(int32_t x,
    int32_t projectedFloorY,
    int32_t projectedCeilingY,
    int32_t nextFloorY,
    int32_t nextCeilY);
  Visplane *FindVisPlane(int16_t height, uint32_t color, int16_t lightLevel);

  void RenderVisPlanes();
  void RenderSegLoop(const seg_t *seg, int16_t topStep, int16_t topFrac, int16_t bottomStep, int16_t bottomFrac);
  void StoreWallRange(const ClipRange &range, const seg_t *seg, const side_t *side, const Player &player);
  void AddSegment(const seg_t *seg, const Player &player);
  Visplane *CheckVisPlane(Visplane *visplane, int16_t start, int16_t end);
  void RenderSSector(const Player &player, const SubSector &subsector);
  void RenderBSPNode(const GameState &gameState, int16_t nodeIndex);

  void ClipSolidWall(int16_t start, int16_t end, const seg_t *seg, const side_t *sidedef, const Player &player);
  void ClipPassWall(int16_t start, int16_t end, const seg_t *seg, const side_t *side, const Player &player);

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

  angle_t m_rw_normalangle;
  angle_t m_rw_angle1;
  float_t m_rw_distance;
  float_t m_rw_scale;
  float_t m_rw_scaleStep;
  int16_t m_rwx;
  int16_t m_rw_stopx;

  FrameBuffer &m_fb;
  std::shared_ptr<Level> m_level;

  uint16_t m_canvasWidth;
  uint16_t m_canvasHeight;
};
