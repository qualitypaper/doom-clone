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
  FixedDiv(CANVAS_WIDTH << (FRAC_BITS - 1), finetangent[FINE_ANGLES / 4 + (HALF_FOV >> ANGLE_TO_FINE_SHIFT)]);


struct drawseg_t
{
  int y, x1, x2;
  fixed_t xStep, yStep;
  fixed_t xFrac, yFrac;

  // TODO: change to texture implementation
  uint32_t color;
};

// Forward declaration
class FrameBuffer;

class Renderer
{
public:
  Renderer(FrameBuffer &_fb, std::shared_ptr<Level> _level, uint16_t canvasWidth, uint16_t canvasHeight);
  void Render(const GameState &gameState);
  void Reset();

  void DrawColumn(int x, int y0, int y1, uint32_t color) const;

private:
  void InitYSlope();
  void InitDistScale();
  void InitViewAngleToX();
  void InitXToViewAngle();

  // plane rendering
  void MapPlane(const Visplane &plane, int y, int x1, int x2) const;
  void MakeSpans(const Visplane &plane, int x, int t1, int b1, int t2, int b2) const;
  void DrawSpan(const drawseg_t &ds) const;

  void ResetSolidSegs();
  void ResetPlanes();
  Visplane *FindVisPlane(fixed_t height, uint32_t color, int16_t lightLevel);
  Visplane *CheckVisPlane(Visplane *visplane, int start, int end);
  void RenderVisPlanes();

  [[nodiscard ]] fixed_t ScaleFromGlobalAngle(angle_t angle) const;
  void RenderSegLoop(const seg_t *seg, fixed_t topStep, fixed_t topFrac, fixed_t bottomStep, fixed_t bottomFrac);
  void StoreWallRange(const ClipRange &range, const seg_t *seg, const side_t *side);
  void RenderSeg(const seg_t *seg);
  void RenderSSector(const SubSector &subsector);
  void RenderBSPNode(int16_t nodeIndex);

  void ClipSolidWall(int start, int end, const seg_t *seg, const side_t *sidedef);
  void ClipPassWall(int start, int end, const seg_t *seg, const side_t *side);

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

private:
  std::vector<int16_t> m_floorClip;
  std::vector<int16_t> m_ceilClip;
  std::vector<ClipRange> m_solidSegs;
  std::vector<Visplane> m_visplanes;

  // distance to each row from the center (player view)
  std::array<fixed_t, CANVAS_HEIGHT> m_ySlope{};
  // essentially 1/cos(angle)
  std::array<int, CANVAS_WIDTH> m_distScale{};

  // maps a fine angle onto screen x
  std::array<int, FINE_ANGLES / 2> m_viewAngleToX{};
  // maps a x into a fine angle
  std::array<angle_t, CANVAS_WIDTH + 1> m_xToViewAngle{};

  Visplane *m_ceilPlane = nullptr, *m_floorPlane = nullptr;

  angle_t m_rwNormalAngle{};
  angle_t m_rwAngle1{};
  fixed_t m_rwDistance{};
  fixed_t m_rwScale{};
  fixed_t m_rwScaleStep{};
  int m_rwx{};
  int m_rwStopX{};

  FrameBuffer &m_fb;
  const Player *m_player;
  std::shared_ptr<Level> m_level;

  uint16_t m_canvasWidth;
  uint16_t m_canvasHeight;
};
