#pragma once

#include "core/framebuffer.h"
#include "defs.h"
#include "tables.h"

#include <array>
#include <memory>
#include <vector>

class SdlWindow;
struct GameState;
struct Level;
struct Player;

static constexpr uint16_t NF_SUBSECTOR = 0x8000;
static constexpr uint16_t MAX_VISPLANES = 128;
static constexpr uint16_t MAX_SEGMENTS = 64;


struct drawspan_t
{
  int y, x1, x2;
  fixed_t xStep, yStep;
  fixed_t xFrac, yFrac;

  // TODO: change to texture implementation
  uint32_t color;
};

struct drawseg_t
{
  const seg_t *seg;
  const side_t *frontSide;
  const side_t *backSide;

  fixed_t topFrac;
  fixed_t topStep;
  fixed_t botFrac;
  fixed_t botStep;

  bool markCeiling;
  bool markFloor;

  fixed_t pixHigh;
  fixed_t pixHighStep;

  fixed_t pixLow;
  fixed_t pixLowStep;
};

class Renderer
{
public:
  Renderer(std::shared_ptr<SdlWindow> sdlWindow, std::shared_ptr<Level> _level);
  void Render(const Player &player);
  void Reset();

  void SetLevel(std::shared_ptr<Level> _level);

  void DrawColumn(int x, int y0, int y1, uint32_t color) const;

private:
  void InitYSlope();
  void InitDistScale();
  void InitViewAngleToX();
  void InitXToViewAngle();

  // plane rendering
  void MapPlane(const Visplane &plane, int y, int x1, int x2) const;
  void MakeSpans(const Visplane &plane, int x, int t1, int b1, int t2, int b2);
  void DrawSpan(drawspan_t &ds) const;

  void ResetSolidSegs();
  void ResetPlanes();
  Visplane *FindVisPlane(fixed_t height, uint32_t color, int16_t lightLevel);
  Visplane *CheckVisPlane(Visplane *visplane, int start, int end);
  void RenderVisPlanes();

  [[nodiscard]] fixed_t ScaleFromGlobalAngle(angle_t angle) const;
  void RenderSegLoop(drawseg_t &ds);
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
  std::vector<fixed_t> m_ySlope;
  // essentially 1/cos(angle)
  std::vector<int> m_distScale;

  // maps a fine angle onto screen x
  std::array<int, FINE_ANGLES / 2> m_viewAngleToX{};
  // maps a x into a fine angle
  std::vector<angle_t> m_xToViewAngle;
  std::vector<int> m_spanStart;

  fixed_t m_focalLength{};
  fixed_t m_centerXFrac{};
  fixed_t m_centerYFrac{};

  Visplane *m_ceilPlane = nullptr, *m_floorPlane = nullptr;

  angle_t m_rwNormalAngle{};
  angle_t m_rwAngle1{};
  fixed_t m_rwDistance{};
  fixed_t m_rwScale{};
  fixed_t m_rwScaleStep{};
  int m_rwx{};
  int m_rwStopX{};

  FrameBuffer m_fb;
  const Player *m_player;
  std::shared_ptr<Level> m_level;
};
