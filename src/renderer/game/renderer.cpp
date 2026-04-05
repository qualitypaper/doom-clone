#include "renderer.h"

#include "config.h"
#include "math_utils.h"
#include "renderer_helper.h"
#include "tables.h"

#include "bsp/bsp.h"
#include "core/framebuffer.h"
#include "core/gameloop.h"

#include <algorithm>
#include <fmt/core.h>
#include <iostream>
#include <utility>

static constexpr int16_t HEIGHT_BITS = 12;
static constexpr int16_t HEIGHT_UNIT = 1 << HEIGHT_BITS;

void Renderer::InitViewAngleToX()
{
  for (size_t i = 0; i < m_viewAngleToX.size(); i++) {
    int t;

    if (finetangent[i] >= FRAC_UNIT * 2) {
      // cover asymptotes
      t = 0;
    } else if (finetangent[i] < -FRAC_UNIT * 2) {
      // cover asymptotes
      t = CANVAS_WIDTH;
    } else {
      // normal case
      t = FixedMul(FOCAL_LENGTH, finetangent[i]);
      // rounding up to the nearest
      t = (CANVAS_CENTERX_FRAC - t + FRAC_UNIT - 1) >> FRAC_BITS;

      if (t < -1) {
        t = -1;
      } else if (t > CANVAS_WIDTH + 1) {
        t = CANVAS_WIDTH;
      }
    }

    m_viewAngleToX[i] = t;
  }
}

void Renderer::InitXToViewAngle()
{
  for (int x = 0; x < m_xToViewAngle.size(); x++) {
    angle_t i = 0;

    // find by brute-forcing the angle mapping to this x
    while (m_viewAngleToX[i] > x) {
      i++;
    }

    // transforming from fine angle into a BAM,
    // then shifting by 90 degrees so that 0 degrees are directly in front of the player
    m_xToViewAngle[x] = (i << ANGLE_TO_FINE_SHIFT) - ANG90;
  }
}


// m_rwDistance and m_rwNormalAngle must be calculated first
fixed_t Renderer::ScaleFromGlobalAngle(const angle_t angle) const
{
  const angle_t anglea = ANG90 + (angle - m_player->angle);
  const angle_t angleb = ANG90 + (angle - m_rwNormalAngle);

  // both sines are allways positive
  const fixed_t sinea = finesine[anglea >> ANGLE_TO_FINE_SHIFT];
  const fixed_t sineb = finesine[angleb >> ANGLE_TO_FINE_SHIFT];
  const fixed_t num = FixedMul(FOCAL_LENGTH, sineb);
  const fixed_t den = FixedMul(m_rwDistance, sinea);

  fixed_t scale;

  if (den > num >> 16) {
    scale = FixedDiv(num, den);

    if (scale > 64 * FRAC_UNIT) {
      scale = 64 * FRAC_UNIT;

    } else if (scale < 256) {
      scale = 256;
    }
  } else {
    scale = 64 * FRAC_UNIT;
  }

  return scale;
}

static constexpr uint32_t MapColor(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t alpha)
{
  return (r << 24) | (g << 16) | (b << 8) | alpha;
}

void Renderer::InitYSlope()
{
  // calc yslope
  for (int i = 0; i < m_ySlope.size(); i++) {
    const fixed_t dy = std::abs(((i - CANVAS_HEIGHT / 2) << FRAC_BITS) + FRAC_UNIT / 2);
    m_ySlope[i] = FixedDiv(FOCAL_LENGTH, dy);
  }
}

// need to calc. xToViewAngle first
void Renderer::InitDistScale()
{
  for (int i = 0; i < m_distScale.size(); i++) {
    const fixed_t angle = (finesine[(m_xToViewAngle[i] + ANG90) >> ANGLE_TO_FINE_SHIFT]);
    m_distScale[i] = FixedDiv(FRAC_UNIT, angle);
  }
}

Renderer::Renderer(FrameBuffer &_fb,
                   std::shared_ptr<Level> _level,
                   const uint16_t canvasWidth,
                   const uint16_t canvasHeight)
  : m_fb(_fb), m_player(nullptr), m_level(std::move(_level)), m_canvasWidth(canvasWidth), m_canvasHeight(canvasHeight)
{
  m_ceilClip.resize(canvasWidth);
  m_floorClip.resize(canvasWidth);
  m_visplanes.reserve(MAX_VISPLANES);
  m_solidSegs.resize(MAX_SEGMENTS);
  ResetSolidSegs();
  InitViewAngleToX();
  InitXToViewAngle();

  InitYSlope();
  InitDistScale();
}

void Renderer::Reset()
{
  ResetPlanes();
  ResetSolidSegs();
}

void Renderer::ResetSolidSegs()
{
  m_solidSegs[0].start = -0x7fffffff;
  m_solidSegs[0].end = -1;
  m_solidSegs[1].start = m_canvasWidth;
  m_solidSegs[1].end = 0x7fffffff;
}

void Renderer::DrawColumn(const int x, const int y0, const int y1, const uint32_t color) const
{
  // scale the point to the window size
  int scaledX = static_cast<int>(SCALE_X * x);
  int transformedY0 = static_cast<int>(SCALE_Y * y0);
  int transformedY1 = static_cast<int>(SCALE_Y * y1);

  scaledX = std::clamp(scaledX, 0, WINDOW_WIDTH - 1);
  transformedY0 = std::clamp(transformedY0, 0, WINDOW_HEIGHT - 1);
  transformedY1 = std::clamp(transformedY1, 0, WINDOW_HEIGHT - 1);

  if (transformedY0 > transformedY1)
    std::swap(transformedY0, transformedY1);

  // using window width as the pitch, because the current
  // implementation doesn't leave any extra pixels
  const uint32_t pitch = this->m_fb.width;

  for (uint32_t i = 0; i < SCALE_X + 1; i++) {
    uint32_t *ptr = this->m_fb.pixels + transformedY0 * pitch + (scaledX + i);

    for (int32_t y = transformedY0; y <= transformedY1; y++) {
      *(uint32_t *)ptr = color;
      ptr += pitch;
    }
  }
}


void Renderer::ClipSolidWall(int start, int end, const seg_t *seg, const side_t *sidedef)
{
  // clip with solid segs
  ClipRange *clipStart = m_solidSegs.data();

  while (clipStart - m_solidSegs.data() < m_solidSegs.size() && clipStart->end < start) {
    clipStart++;
  }

  ClipRange *next;
  if (start < clipStart->start) {
    if (end < clipStart->start - 1) {
      // no solid segs in the way, render the whole seg
      StoreWallRange({ start, end }, seg, sidedef);

      next = m_solidSegs.end().base();

      // moves everything to the right by one
      // and creates an empty slot exactly at "start"
      while (next != clipStart) {
        *next = *(next - 1);
        next--;
      }

      next->start = start;
      next->end = end;
      return;
    }

    // there is a fragment above *start
    StoreWallRange({ start, clipStart->start - 1 }, seg, sidedef);
    clipStart->start = start;
  }

  // bottom contained in start
  if (end <= clipStart->end) {
    return;
  }

  next = clipStart;

  while (next + 1 - m_solidSegs.data() < m_solidSegs.size() && end >= (next + 1)->start - 1) {
    // there is a fragment between two posts
    StoreWallRange({ next->end + 1, (next + 1)->start - 1 }, seg, sidedef);
    next++;

    if (end <= next->end) {
      // bottom is contained in next
      clipStart->end = next->end;

      // crunch
      if (next == clipStart)
        return;

      while (clipStart < m_solidSegs.data() + m_solidSegs.size() && next++ != m_solidSegs.data() + m_solidSegs.size()) {
        // remove a post
        *(++clipStart) = *next;
      }

      return;
    }
  }

  // there is a fragment after next
  StoreWallRange({ next->end + 1, end }, seg, sidedef);
  next->end = end;
}

void Renderer::ClipPassWall(int start, int end, const seg_t *seg, const side_t *side)
{
  // Find the first range that touches the range
  //  (adjacent pixels are touching).
  ClipRange *clipStart = m_solidSegs.data();

  while (clipStart->end < start - 1) {
    clipStart++;
  }

  if (start < clipStart->start) {
    if (end < clipStart->start - 1) {
      // Post is entirely visible (above start).
      StoreWallRange({ start, end }, seg, side);
      return;
    }

    // There is a fragment above *start.
    StoreWallRange({ start, static_cast<int16_t>(clipStart->start - 1) }, seg, side);
  }

  // Bottom contained in start?
  if (end <= clipStart->end) {
    return;
  }

  while (end >= (clipStart + 1)->start - 1) {
    // There is a fragment between two posts.
    StoreWallRange({ clipStart->end + 1, (clipStart + 1)->start - 1 }, seg, side);
    clipStart++;

    if (end <= clipStart->end) {
      // Bottom is contained in next.
      return;
    }
  }

  // There is a fragment after *next.
  StoreWallRange({ clipStart->end + 1, end }, seg, side);
}

void Renderer::RenderSeg(const seg_t *seg)
{
  const line_t &ld = *seg->line;

  angle_t angle1 = PointToAngle(seg->start->x, seg->start->y, m_player->x, m_player->y);
  angle_t angle2 = PointToAngle(seg->end->x, seg->end->y, m_player->x, m_player->y);

  const angle_t span = angle1 - angle2;

  if (span >= ANG180) {
    return;
  }

  m_rwAngle1 = angle1;
  angle1 -= m_player->angle;
  angle2 -= m_player->angle;

  angle_t tspan = angle1 + HALF_FOV;

  if (tspan > FOV) {
    // totally of the left edge?
    if (tspan - FOV >= span) {
      return;
    }

    angle1 = HALF_FOV;
  }

  tspan = HALF_FOV - angle2;
  if (tspan > FOV) {
    // totally of the right edge?
    if (tspan - FOV >= span) {
      return;
    }

    angle2 = -HALF_FOV;
  }

  angle1 = (angle1 + FOV) >> ANGLE_TO_FINE_SHIFT;
  angle2 = (angle2 + FOV) >> ANGLE_TO_FINE_SHIFT;

  int x1 = m_viewAngleToX[angle1];
  int x2 = m_viewAngleToX[angle2];

  if (x1 == x2)
    return;

  const bool isBackSide = seg->side;

  const side_t *frontSide = isBackSide ? ld.backSide : ld.frontSide;
  const side_t *backSide = isBackSide ? ld.frontSide : ld.backSide;

  if (!frontSide) {
    throw std::runtime_error("frontSidedefIndex == -1 -> The frontSidedef mustn't be empty/null.");
  }

  if (!backSide) {
    ClipSolidWall(x1, x2, seg, frontSide);
  } else {
    ClipPassWall(x1, x2, seg, backSide);
  }
}

void Renderer::RenderSegLoop(const seg_t *seg,
                             const fixed_t topStep,
                             fixed_t topFrac,
                             const fixed_t bottomStep,
                             fixed_t bottomFrac)
{
  for (int x = m_rwx; x < std::min(m_rwStopX, static_cast<int>(CANVAS_WIDTH)); x++) {
    int yl = (topFrac + HEIGHT_UNIT - 1) >> HEIGHT_BITS;

    if (yl < m_ceilClip[x] + 1) {
      yl = m_ceilClip[x] + 1;
    }

    // mark ceiling
    int top = m_ceilClip[x] + 1;
    int bottom = yl - 1;

    if (bottom >= m_floorClip[x]) {
      bottom = m_floorClip[x] - 1;
    }

    if (top <= bottom && m_ceilPlane) {
      m_ceilPlane->top[x] = top;
      m_ceilPlane->bottom[x] = bottom;
    }

    int yh = (bottomFrac + HEIGHT_UNIT - 1) >> HEIGHT_BITS;

    if (yh >= m_floorClip[x]) {
      yh = m_floorClip[x] - 1;
    }

    // mark floor
    top = yh + 1;
    bottom = m_floorClip[x] - 1;

    if (top <= m_ceilClip[x]) {
      top = m_ceilClip[x] + 1;
    }

    if (top <= bottom && m_floorPlane) {
      m_floorPlane->top[x] = top;
      m_floorPlane->bottom[x] = bottom;
    }

    if (!seg->line->backSide) {
      // test version, in order to understand which wall is whichdddddddd
      ImVec4 color = ImGui::ColorConvertU32ToFloat4(seg->line->frontSide->sector->color);
      float index = (seg->line->frontSide - m_level->sidedefs.data()) / static_cast<float>(m_level->sidedefs.size());
      color.x += index;
      color.y += index;
      color.z += index;

      DrawColumn(x,
                 yl,
                 yh,
                 MapColor(static_cast<uint8_t>(color.x * 255),
                          static_cast<uint8_t>(color.y * 255),
                          static_cast<uint8_t>(color.z * 255),
                          255));
      m_ceilClip[x] = static_cast<int16_t>(m_canvasHeight);
      m_floorClip[x] = -1;
    } else {
      if (seg->line->type == LineDefType::REGULAR) {
        // portal
        if (yl > m_ceilClip[x])
          m_ceilClip[x] = static_cast<int16_t>(yl);
        if (yh < m_floorClip[x])
          m_floorClip[x] = static_cast<int16_t>(yh);

      } else if (seg->line->type == LineDefType::DOOR) {
        // TODO: make a door renderer
      }
    }

    topFrac += topStep;
    bottomFrac += bottomStep;
  }
}

void Renderer::StoreWallRange(const ClipRange &range, const seg_t *seg, const side_t *side)
{
  // calculate rw_distance for scale calculation
  m_rwNormalAngle = seg->angle + ANG90;

  angle_t offsetAngle = std::abs(static_cast<long long>(m_rwNormalAngle) - static_cast<long long>(m_rwAngle1));

  // TODO: figure out why Doom used this
  // if (offsetAngle > ANG90) {
  //   offsetAngle = ANG90;
  // }

  const angle_t disAngle = ANG90 - offsetAngle;
  const fixed_t hyp = PointToDist(seg->line->start->x, seg->line->start->y, *m_player);
  const fixed_t sineval = finesine[disAngle >> ANGLE_TO_FINE_SHIFT];
  m_rwDistance = FixedMul(hyp, sineval);

  m_rwx = range.start;
  m_rwStopX = range.end + 1;

  m_rwScale = ScaleFromGlobalAngle(m_player->angle + m_xToViewAngle[range.start]);

  if (range.end > range.start) {
    const fixed_t scale2 = ScaleFromGlobalAngle(m_player->angle + m_xToViewAngle[range.end]);
    m_rwScaleStep = (scale2 - m_rwScale) / (range.end - range.start);
  } else {
    m_rwScaleStep = 0;
  }

  fixed_t wordTop = side->sector->ceilingHeight - m_player->z;
  fixed_t wordBottom = side->sector->floorHeight - m_player->z;

  wordTop >>= 4;
  wordBottom >>= 4;

  fixed_t topStep = -FixedMul(m_rwScaleStep, wordTop);
  fixed_t topFrac = (CANVAS_CENTERY_FRAC >> 4) - FixedMul(m_rwScale, wordTop);

  fixed_t bottomStep = -FixedMul(m_rwScaleStep, wordBottom);
  fixed_t bottomFrac = (CANVAS_CENTERY_FRAC >> 4) - FixedMul(m_rwScale, wordBottom);

  bool markCeiling = true;
  bool markFloor = true;

  if (!seg->line->backSide) {
    markCeiling = markFloor = true;
  } else {
    if (side->sector->floorHeight >= m_player->z) {
      markFloor = false;
    }

    if (side->sector->ceilingHeight <= m_player->z) {
      markCeiling = false;
    }
  }

  if (markCeiling)
    m_ceilPlane = CheckVisPlane(m_ceilPlane, m_rwx, m_rwStopX - 1);

  if (markFloor)
    m_floorPlane = CheckVisPlane(m_floorPlane, m_rwx, m_rwStopX - 1);

  RenderSegLoop(seg, topStep, topFrac, bottomStep, bottomFrac);
}

void Renderer::RenderSSector(const SubSector &subsector)
{
  int count = subsector.segCount;
  const Sector *frontsector = subsector.sector;
  const seg_t *seg = &m_level->segments[subsector.firstSegIndex];

  if (frontsector->floorHeight < m_player->z) {
    m_floorPlane = FindVisPlane(frontsector->floorHeight, frontsector->color, frontsector->lightLevel);
  } else {
    m_floorPlane = nullptr;
  }

  if (frontsector->ceilingHeight > m_player->z) {
    m_ceilPlane = FindVisPlane(frontsector->ceilingHeight, frontsector->color, frontsector->lightLevel);
  } else {
    m_ceilPlane = nullptr;
  }

  // render segments
  while (count--) {
    RenderSeg(seg);
    seg++;
  }
}

void Renderer::RenderBSPNode(const int16_t nodeIndex)
{
  if (nodeIndex & NF_SUBSECTOR) {
    // leaf node
    const SubSector &subsector = m_level->subsectors[nodeIndex & static_cast<uint16_t>(~NF_SUBSECTOR)];

    RenderSSector(subsector);

    return;
  }

  const Node &node = m_level->nodes[nodeIndex];
  const bool side = PointOnSide({ m_player->x, m_player->y }, node);

  if (side) {
    RenderBSPNode(node.leftChild);
  } else {
    RenderBSPNode(node.rightChild);
  }

  const int16_t farNodeIndex = side ? node.rightChild : node.leftChild;

  // TODO: make a culling method
  RenderBSPNode(farNodeIndex);
}

void Renderer::Render(const GameState &gameState)
{
  this->m_player = &gameState.playerState;
  Reset();

  if (m_level->nodes.empty()) {
    // Entire level is a single subsector (no BSP splits were needed)
    if (!m_level->subsectors.empty()) {
      const SubSector &subsector = m_level->subsectors[0];

      for (int16_t i = subsector.firstSegIndex; i < subsector.firstSegIndex + subsector.segCount; i++) {
        RenderSeg(m_level->segments.data() + i);
      }
    }
  } else {
    RenderBSPNode(0);
  }

#if 0
  RenderVisPlanes();
#endif

  m_fb.update();
}
