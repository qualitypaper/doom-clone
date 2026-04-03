#pragma once

#include "../../../include/math_utils.h"
#include "../../../include/tables.h"
#include "../../bsp/bsp.h"
#include "../../core/gameloop.h"

#include <vector>

static constexpr uint16_t NF_SUBSECTOR = 0x8000;
static constexpr uint16_t MAX_VISPLANES = 128;
static constexpr uint16_t MAX_SEGMENTS = 64;

static const fixed_t FOCAL_LENGTH =
  FixedDiv(config::CANVAS_WIDTH << (FRAC_BITS - 1), finetangent[FINE_ANGLES / 4 + (HALF_FOV >> ANGLE_TO_FINE_SHIFT)]);

// Forward declaration
class FrameBuffer;

class Renderer
{
public:
  Renderer(FrameBuffer &_fb, std::shared_ptr<Level> _level, uint16_t canvasWidth, uint16_t canvasHeight);
  void Render(const GameState &gameState);
  void ResetClippingArrays();
  void ResetSolidSegs();

  void DrawColumn(int x, int y0, int y1, uint32_t color) const;

private:
  void InitViewAngleToX();
  void InitXToViewAngle();


  Visplane *FindVisPlane(fixed_t height, uint32_t color, int16_t lightLevel);
  Visplane *CheckVisPlane(Visplane *visplane, int start, int end);
  void RenderVisPlanes();

  fixed_t ScaleFromGlobalAngle(angle_t angle, const Player &player) const;
  void RenderSegLoop(const seg_t *seg, fixed_t topStep, fixed_t topFrac, fixed_t bottomStep, fixed_t bottomFrac);
  void StoreWallRange(const ClipRange &range, const seg_t *seg, const side_t *side, const Player &player);
  void RenderSeg(const seg_t *seg, const Player &player);
  void RenderSSector(const Player &player, const SubSector &subsector);
  void RenderBSPNode(const GameState &gameState, int16_t nodeIndex);

  void ClipSolidWall(int start, int end, const seg_t *seg, const side_t *sidedef, const Player &player);
  void ClipPassWall(int start, int end, const seg_t *seg, const side_t *side, const Player &player);

  static bool PointOnSide(const Vertex v, const Node &node)
  {
    // Calculate vector from the partition line's origin to the player
    const fixed_t dx = v.x - node.x;
    const fixed_t dy = v.y - node.y;

    // 2D Cross Product
    const fixed_t left = FixedMul(node.dy >> FRAC_BITS, dx);
    const fixed_t right = FixedMul(dy, node.dx >> FRAC_BITS);

    return left < right;
  }

  [[nodiscard]] int ProjectZ(double_t z, double_t inv_y) const;
  [[nodiscard]] int ProjectX(double_t x, double_t inv_y) const;

private:
  std::vector<int16_t> m_floorClip;
  std::vector<int16_t> m_ceilClip;
  // newEnd is one past the last clip range
  ClipRange* m_newEnd;
  std::vector<ClipRange> m_solidSegs;
  std::vector<Visplane> m_visplanes;

  // maps a fine angle onto screen x
  std::array<int, FINE_ANGLES / 2> m_viewAngleToX{};
  // maps a x into a fine angle
  std::array<angle_t, config::CANVAS_WIDTH + 1> m_xToViewAngle{};

  Visplane *m_ceilPlane = nullptr, *m_floorPlane = nullptr;

  angle_t m_rw_normalAngle{};
  angle_t m_rw_angle1{};
  fixed_t m_rw_distance{};
  fixed_t m_rw_scale{};
  fixed_t m_rw_scaleStep{};
  int m_rwx{};
  int m_rwStopX{};

  FrameBuffer &m_fb;
  std::shared_ptr<Level> m_level;

  uint16_t m_canvasWidth;
  uint16_t m_canvasHeight;
};
