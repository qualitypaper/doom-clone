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
  const int screenWidth = static_cast<int>(m_fb.width);

  for (size_t i = 0; i < m_viewAngleToX.size(); i++) {
    int t;

    if (finetangent[i] >= FRAC_UNIT * 2) {
      // cover asymptotes
      t = 0;
    } else if (finetangent[i] < -FRAC_UNIT * 2) {
      // cover asymptotes
      t = screenWidth;
    } else {
      // normal case
      t = FixedMul(m_focalLength, finetangent[i]);
      // rounding up to the nearest
      t = (m_centerXFrac - t + FRAC_UNIT - 1) >> FRAC_BITS;

      if (t < -1) {
        t = -1;
      } else if (t > screenWidth + 1) {
        t = screenWidth;
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
    while (i + 1 < m_viewAngleToX.size() && m_viewAngleToX[i] > x) {
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
  const fixed_t num = FixedMul(m_focalLength, sineb);
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
    const fixed_t dy = std::abs(((i - static_cast<int>(m_fb.height) / 2) << FRAC_BITS) + FRAC_UNIT / 2);
    m_ySlope[i] = FixedDiv(m_focalLength, dy);
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

Renderer::Renderer(std::shared_ptr<SdlWindow> sdlWindow, std::shared_ptr<Level> _level)
  : m_fb(sdlWindow), m_player(nullptr), m_level(std::move(_level))
{
  m_centerXFrac = static_cast<fixed_t>(m_fb.width) << (FRAC_BITS - 1);
  m_centerYFrac = static_cast<fixed_t>(m_fb.height) << (FRAC_BITS - 1);
  m_focalLength = FixedDiv(static_cast<fixed_t>(m_fb.width) << (FRAC_BITS - 1),
                           finetangent[FINE_ANGLES / 4 + (HALF_FOV >> ANGLE_TO_FINE_SHIFT)]);

  m_ceilClip.resize(m_fb.width);
  m_floorClip.resize(m_fb.width);
  m_ySlope.resize(m_fb.height);
  m_distScale.resize(m_fb.width);
  m_xToViewAngle.resize(m_fb.width + 1);
  m_spanStart.resize(m_fb.height);

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
void Renderer::SetLevel(std::shared_ptr<Level> _level) { this->m_level = std::move(_level); }

void Renderer::ResetSolidSegs()
{
  m_solidSegs[0].start = -0x7fffffff;
  m_solidSegs[0].end = -1;
  m_solidSegs[1].start = m_fb.width;
  m_solidSegs[1].end = 0x7fffffff;
}

void Renderer::DrawColumn(int x, int y0, int y1, const uint32_t color) const
{
  // scale the point to the window size
  x = std::clamp(x, 0, m_fb.width - 1);
  y0 = std::clamp(y0, 0, m_fb.height - 1);
  y1 = std::clamp(y1, 0, m_fb.height - 1);

  if (y0 > y1)
    std::swap(y0, y1);

  // using window width as the pitch, because the current
  // implementation doesn't leave any extra pixels
  const uint32_t pitch = m_fb.width;

  uint32_t *ptr = m_fb.pixels + y0 * pitch + x;

  for (int32_t y = y0; y <= y1; y++) {
    *(uint32_t *)ptr = color;
    ptr += pitch;
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
    ClipPassWall(x1, x2, seg, frontSide);
  }
}

void Renderer::RenderSegLoop(drawseg_t &ds)
{
  for (int x = m_rwx; x < std::min(m_rwStopX, static_cast<int>(m_fb.width)); x++) {
    int yl = (ds.topFrac + HEIGHT_UNIT - 1) >> HEIGHT_BITS;

    if (yl < m_ceilClip[x] + 1) {
      yl = m_ceilClip[x] + 1;
    }

    // mark ceiling
    if (ds.markCeiling) {
      int top = m_ceilClip[x];
      int bottom = yl - 1;

      if (bottom >= m_floorClip[x]) {
        bottom = m_floorClip[x] - 1;
      }

      if (top <= bottom && m_ceilPlane) {
        m_ceilPlane->top[x] = top;
        m_ceilPlane->bottom[x] = bottom;
      }
    }


    int yh = (ds.botFrac + HEIGHT_UNIT - 1) >> HEIGHT_BITS;

    if (yh >= m_floorClip[x]) {
      yh = m_floorClip[x] - 1;
    }

    // mark floor
    if (ds.markFloor) {
      int top = yh + 1;
      int bottom = m_floorClip[x] - 1;

      if (top <= m_ceilClip[x]) {
        top = m_ceilClip[x] + 1;
      }

      if (top <= bottom && m_floorPlane) {
        m_floorPlane->top[x] = top;
        m_floorPlane->bottom[x] = bottom;
      }
    }

    const uint32_t color = ds.frontSide->sector->color;

    if (!ds.backSide) {
      // test version, in order to understand which wall is which
      ImVec4 _color = ImGui::ColorConvertU32ToFloat4(color);
      float index = (ds.frontSide - m_level->sidedefs.data()) / static_cast<float>(m_level->sidedefs.size());
      _color.x += index;
      _color.y += index;
      _color.z += index;

      DrawColumn(x,
                 yl,
                 yh,
                 MapColor(static_cast<uint8_t>(_color.x * 255),
                          static_cast<uint8_t>(_color.y * 255),
                          static_cast<uint8_t>(_color.z * 255),
                          255));

      m_ceilClip[x] = static_cast<int16_t>(m_fb.height);
      m_floorClip[x] = -1;
    } else {
      if (ds.seg->line->type == LineDefType::REGULAR) {
        // portal

        // top wall
        int mid = ds.pixHigh >> HEIGHT_BITS;
        ds.pixHigh += ds.pixHighStep;

        if (mid >= m_floorClip[x]) {
          mid = m_floorClip[x] - 1;
        }

        if (mid >= yl) {
          DrawColumn(x, mid, yl, MapColor(0, 255, 0, 255));
          m_ceilClip[x] = mid;
        } else {
          m_ceilClip[x] = yl - 1;
        }

        // bottom wall
        mid = (ds.pixLow + HEIGHT_UNIT - 1) >> HEIGHT_BITS;
        ds.pixLow += ds.pixLowStep;

        if (mid <= m_ceilClip[x]) {
          mid = m_ceilClip[x] + 1;
        }

        if (mid <= yh) {
          DrawColumn(x, mid, yh, MapColor(0, 255, 0, 255));
          m_floorClip[x] = mid;
        } else {
          m_floorClip[x] = yh + 1;
        }

      } else if (ds.seg->line->type == LineDefType::DOOR) {
        // TODO: make a door renderer
      }
    }

    m_rwScale += m_rwScaleStep;
    ds.topFrac += ds.topStep;
    ds.botFrac += ds.botStep;
  }
}

void Renderer::StoreWallRange(const ClipRange &range, const seg_t *seg, const side_t *side)
{
  const int maxX = static_cast<int>(m_fb.width) - 1;
  const int clampedStart = std::clamp(range.start, 0, maxX);
  const int clampedEnd = std::clamp(range.end, 0, maxX);

  if (clampedStart > clampedEnd) {
    return;
  }

  // calculate rw_distance for scale calculation
  m_rwNormalAngle = seg->angle + ANG90;

  angle_t offsetAngle = std::abs(static_cast<long long>(m_rwNormalAngle) - static_cast<long long>(m_rwAngle1));

  // TODO: figure out why Doom used this
  if (offsetAngle > ANG90) {
    offsetAngle = ANG90;
  }

  const angle_t disAngle = ANG90 - offsetAngle;
  const fixed_t hyp = PointToDist(seg->line->start->x, seg->line->start->y, *m_player);
  const fixed_t sineval = finesine[disAngle >> ANGLE_TO_FINE_SHIFT];
  m_rwDistance = FixedMul(hyp, sineval);

  m_rwx = clampedStart;
  m_rwStopX = clampedEnd + 1;

  m_rwScale = ScaleFromGlobalAngle(m_player->angle + m_xToViewAngle[clampedStart]);

  if (clampedEnd > clampedStart) {
    const fixed_t scale2 = ScaleFromGlobalAngle(m_player->angle + m_xToViewAngle[clampedEnd]);
    m_rwScaleStep = (scale2 - m_rwScale) / (clampedEnd - clampedStart);
  } else {
    m_rwScaleStep = 0;
  }

  fixed_t worldTop = side->sector->ceilingHeight - m_player->z;
  fixed_t worldBottom = side->sector->floorHeight - m_player->z;

  drawseg_t ds{ .seg = seg,
                .frontSide = side,
                .backSide = !seg->side ? seg->line->backSide : seg->line->frontSide };

  worldTop >>= 4;
  worldBottom >>= 4;

  ds.topStep = -FixedMul(m_rwScaleStep, worldTop);
  ds.topFrac = (m_centerYFrac >> 4) - FixedMul(m_rwScale, worldTop);

  ds.botStep = -FixedMul(m_rwScaleStep, worldBottom);
  ds.botFrac = (m_centerYFrac >> 4) - FixedMul(m_rwScale, worldBottom);

  fixed_t worldHigh;
  fixed_t worldLow;

  if (ds.backSide) {
    worldHigh = ds.backSide->sector->ceilingHeight - m_player->z;
    worldLow = ds.backSide->sector->floorHeight - m_player->z;

    worldHigh >>= 4;
    worldLow >>= 4;

    ds.pixHighStep = -FixedMul(m_rwScaleStep, worldHigh);
    ds.pixLowStep = -FixedMul(m_rwScaleStep, worldLow);
  } else {
    worldHigh = 0;
    worldLow = 0;
  }

  ds.pixHigh = (m_centerYFrac >> 4) - FixedMul(m_rwScale, worldHigh);
  ds.pixLow = (m_centerYFrac >> 4) - FixedMul(m_rwScale, worldLow);

  bool markCeiling = true;
  bool markFloor = true;

  if (!ds.backSide) {
    markCeiling = markFloor = true;
  } else {
    if (side->sector->floorHeight >= m_player->z) {
      markFloor = false;
    }

    if (side->sector->ceilingHeight <= m_player->z) {
      markCeiling = false;
    }
  }

  ds.markFloor = markFloor;
  ds.markCeiling = markCeiling;

  if (markCeiling)
    m_ceilPlane = CheckVisPlane(m_ceilPlane, m_rwx, m_rwStopX - 1);

  if (markFloor)
    m_floorPlane = CheckVisPlane(m_floorPlane, m_rwx, m_rwStopX - 1);


  RenderSegLoop(ds);
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

void Renderer::Render(const Player &player)
{
  this->m_player = &player;
  Reset();

  if (m_level->nodes.empty()) {
    // Entire level is a single subsector (no BSP splits were needed)
    if (!m_level->subsectors.empty()) {
      const SubSector &subsector = m_level->subsectors[0];

      RenderSSector(subsector);
    }
  } else {
    RenderBSPNode(0);
  }

#if 1
  RenderVisPlanes();
#endif

  m_fb.update();
}
