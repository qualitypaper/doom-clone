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
#include <utility>

static constexpr int16_t HEIGHT_BITS = 12;
static constexpr int16_t HEIGHT_UNIT = 1 << HEIGHT_BITS;

void Renderer::InitViewAngleToX()
{
  for (size_t i = 0; i < m_viewangletox.size(); i++) {
    int t;

    if (finetangent[i] >= FRAC_UNIT * 2) {
      // cover asymptotes
      t = -1;
    } else if (finetangent[i] < -FRAC_UNIT * 2) {
      // cover asymptotes
      t = config::CANVAS_WIDTH + 1;
    } else {
      // normal case
      t = FixedMul(FOCAL_LENGTH, finetangent[i]);
      // rounding up to the nearest
      t = (config::CANVAS_CENTERX_FRAC - t + FRAC_UNIT - 1) >> FRAC_BITS;

      if (t < -1) {
        t = -1;
      } else if (t > config::CANVAS_WIDTH + 1) {
        t = config::CANVAS_WIDTH + 1;
      }
    }

    m_viewangletox[i] = t;
  }
}

void Renderer::InitXToViewAngle()
{
  for (int x = 0; x < m_xtoviewangle.size(); x++) {
    angle_t i = 0;

    // find by brute-forcing the angle mapping to this x
    while (viewangletox[i] > x) {
      i++;
    }

    // transforming from fine angle into a BAM,
    // then shifting by 90 degrees so that 0 degrees are directly in front of the player
    m_xtoviewangle[x] = (i << ANGLE_TO_FINE_SHIFT) - ANG90;
  }
}


static float_t ScaleFromGlobalAngle(angle_t angle) {}

static constexpr uint32_t MapColor(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t alpha)
{
  return (r << 24) | (g << 16) | (b << 8) | alpha;
}

Renderer::Renderer(FrameBuffer &_fb,
                   std::shared_ptr<Level> _level,
                   const uint16_t canvasWidth,
                   const uint16_t canvasHeight)
  : m_fb(_fb), m_level(std::move(_level)), m_canvasWidth(canvasWidth), m_canvasHeight(canvasHeight)
{
  m_ceilclip.resize(canvasWidth);
  m_floorclip.resize(canvasWidth);
  m_visplanes.reserve(MAX_VISPLANES);
  m_solidsegs.resize(MAX_SEGMENTS);

  InitViewAngleToX();
  InitXToViewAngle();
}

void Renderer::ResetClippingArrays()
{
  std::ranges::fill(m_ceilclip, 0);
  std::ranges::fill(m_floorclip, m_canvasHeight - 1);
  std::ranges::fill(m_solidsegs, ClipRange{});
  std::ranges::fill(m_visplanes, Visplane{});
}

void Renderer::DrawColumn(const int32_t x, const int32_t y0, const int32_t y1, const uint32_t color) const
{
  // scale the point to the window size
  const int32_t scaledX = static_cast<int32_t>(config::SCALE_X * x);
  int32_t transformedY0 = static_cast<int32_t>(config::SCALE_Y * y0);
  int32_t transformedY1 = static_cast<int32_t>(config::SCALE_Y * y1);

  if (scaledX < 0 || scaledX > config::WINDOW_WIDTH - 1 || transformedY0 < 0
      || transformedY0 > config::WINDOW_HEIGHT - 1 || transformedY1 < 0 || transformedY1 > config::WINDOW_HEIGHT - 1) {
    return;
  }

  if (transformedY0 > transformedY1)
    std::swap(transformedY0, transformedY1);

  // using window width as the pitch, because the current
  // implementation doesn't leave any extra pixels
  const uint32_t pitch = this->m_fb.width;

  transformedY0 = std::max(0, transformedY0);
  transformedY1 = std::min(this->m_fb.height - 1, transformedY1);

  for (uint32_t i = 0; i < config::SCALE_X + 1; i++) {
    uint32_t *ptr = this->m_fb.pixels + (transformedY0)*pitch + (scaledX + i);

    for (int32_t y = transformedY0; y <= transformedY1; y++) {
      *(uint32_t *)ptr = color;
      ptr += pitch;
    }
  }
}

void Renderer::DrawSolidWall(const int32_t x,
                             const int32_t projectedCeilingZ,
                             const int32_t projectedFloorZ,
                             const uint32_t color)
{
  const int32_t drawTop = std::max(projectedCeilingZ, m_ceilclip[x]);
  const int32_t drawBottom = std::min(projectedFloorZ, m_floorclip[x]);

  if (drawTop <= drawBottom) {
    DrawColumn(x, drawTop, drawBottom, color);

    m_ceilclip[x] = drawBottom;
    m_floorclip[x] = drawTop;
  }
}

void Renderer::Render(const GameState &gameState)
{
  if (m_level->nodes.empty()) {
    // Entire level is a single subsector (no BSP splits were needed)
    if (!m_level->subsectors.empty()) {
      const SubSector &subsector = m_level->subsectors[0];

      for (int16_t i = subsector.firstSegIndex; i < subsector.firstSegIndex + subsector.segCount; i++) {
        AddSegment(m_level->segments.data() + i, gameState.playerState);
      }
    }
  } else {
    RenderBSPNode(gameState, 0);
  }

  RenderVisPlanes();

  m_fb.update();
}

void Renderer::ClipSolidWall(int16_t start, int16_t end, const seg_t *seg, const side_t *sidedef, const Player &player)
{
  // clip with solid segs
  ClipRange *clipStart = m_solidsegs.data();
  ClipRange *next;

  while (clipStart->end < start) {
    clipStart++;
  }

  if (start < clipStart->start) {
    if (end < clipStart->start - 1) {
      // no solid segs in the way, render the whole seg
      StoreWallRange({ start, end }, seg, sidedef, player);

      m_solidsegs.emplace({});
      next = m_solidsegs.data() + m_solidsegs.size();

      // moves everything to the right by one
      // and creates an empty slot exactly at "start"
      while (next != clipStart) {
        *next = *(next - 1);
        next--;
      }

      next->start = start;
      next->end = end;
    }

    // there is a fragment above *start
    StoreWallRange({ start, static_cast<int16_t>(clipStart->start - 1) }, seg, sidedef, player);
    clipStart->start = start;
  }

  // bottom contained in start
  if (end <= clipStart->end) {
    return;
  }

  next = clipStart;

  while (end >= (next + 1)->start - 1) {
    // there is a fragment between two posts
    StoreWallRange(
      { static_cast<int16_t>(next->end + 1), static_cast<int16_t>((next + 1)->start - 1) }, seg, sidedef, player);
    next++;

    if (end <= next->end) {
      // bottom is contained in next
      clipStart->end = next->end;

      // crunch
      if (next == clipStart)
        return;

      while (next++ != m_solidsegs.data() + m_solidsegs.size()) {
        // remove a post
        *(++clipStart) = *next;
      }
    }
  }

  // there is a fragment after next
  StoreWallRange({ static_cast<int16_t>(next->end + 1), end }, seg, sidedef, player);
  next->end = end;
}

void Renderer::ClipPassWall(const int16_t start,
                            const int16_t end,
                            const seg_t *seg,
                            const side_t *side,
                            const Player &player)
{

  // Find the first range that touches the range
  //  (adjacent pixels are touching).
  ClipRange *clipStart = m_solidsegs.data();

  while (clipStart->end < start - 1) {
    clipStart++;
  }

  if (start < clipStart->start) {
    if (end < clipStart->start - 1) {
      // Post is entirely visible (above start).
      StoreWallRange({ start, end }, seg, side, player);
      return;
    }

    // There is a fragment above *start.
    StoreWallRange({ start, static_cast<int16_t>(clipStart->start - 1) }, seg, side, player);
  }

  // Bottom contained in start?
  if (end <= clipStart->end) {
    return;
  }

  while (end >= (clipStart + 1)->start - 1) {
    // There is a fragment between two posts.
    StoreWallRange({ static_cast<int16_t>(clipStart->end + 1), static_cast<int16_t>((clipStart + 1)->start - 1) },
                   seg,
                   side,
                   player);
    clipStart++;

    if (end <= clipStart->end) {
      // Bottom is contained in next.
      return;
    }
  }

  // There is a fragment after *next.
  StoreWallRange({ static_cast<int16_t>(clipStart->end + 1), end }, seg, side, player);
}

void Renderer::AddSegment(const seg_t *seg, const Player &player)
{
  const line_t &ld = *seg->line;

  angle_t angle1 = PointToAngle(seg->start->x, seg->start->y, player.x, player.y);
  angle_t angle2 = PointToAngle(seg->end->x, seg->end->y, player.x, player.y);

  const angle_t span = angle1 - angle2;

  if (span >= ANG180) {
    return;
  }

  m_rw_angle1 = angle1;
  angle1 -= player.angle;
  angle2 -= player.angle;

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

  angle1 = (angle1 + HALF_FOV) >> ANGLE_TO_FINE_SHIFT;
  angle2 = (angle2 + HALF_FOV) >> ANGLE_TO_FINE_SHIFT;

  const int16_t x1 = viewangletox[angle1];
  const int16_t x2 = viewangletox[angle2];

  const bool isBackSide = seg->side;

  const side_t *frontSide = isBackSide ? ld.backSide : ld.frontSide;
  const side_t *backSide = isBackSide ? ld.frontSide : ld.backSide;

  if (!frontSide) {
    throw std::runtime_error("frontSidedefIndex == -1 -> The frontSidedef mustn't be empty.");
  }

  if (!backSide) {
    ClipSolidWall(x1, x2, seg, frontSide, player);
  } else {
    ClipPassWall(x1, x2, seg, backSide, player);
  }
}

Visplane *Renderer::CheckVisPlane(Visplane *visplane, const int16_t start, const int16_t end) { return visplane; }

void Renderer::RenderVisPlanes() {}

void Renderer::RenderSegLoop(const seg_t *seg, int16_t topStep, int16_t topFrac, int16_t bottomStep, int16_t bottomFrac)
{
  int16_t top, bottom;

  for (int16_t x = m_rwx; x <= m_rw_stopx; x++) {
    int16_t yl = topFrac;

    if (yl < m_ceilclip[x] + 1) {
      yl = m_ceilclip[x] + 1;
    }

    // mark ceiling
    top = m_ceilclip[x] + 1;
    bottom = yl - 1;

    if (bottom >= m_floorclip[x]) {
      bottom = m_floorclip[x] - 1;
    }

    if (top <= bottom) {
      m_ceilplane->top[x] = top;
      m_ceilplane->bottom[x] = bottom;
    }


    int16_t yh = bottomFrac;

    if (yh >= m_floorclip[x]) {
      yh = m_floorclip[x] - 1;
    }

    // mark floor
    top = yh + 1;
    bottom = m_floorclip[x] - 1;

    if (top <= m_ceilclip[x]) {
      top = m_ceilclip[x] + 1;
    }

    if (top <= bottom) {
      m_floorplane->top[x] = top;
      m_floorplane->bottom[x] = bottom;
    }


    if (!seg->line->backSide ^ !seg->line->backSide) {
      // one-sided line
      DrawColumn(x, top, bottom, seg->line->frontSide->sector->color);
    } else {
      // TODO: portals
    }

    topFrac += topStep;
    bottomFrac += bottomStep;
  }
}

void Renderer::StoreWallRange(const ClipRange &range, const seg_t *seg, const side_t *side, const Player &player)
{
  // calculate rw_distance for scale calculation
  m_rw_normalangle = seg->angle + ANG90;
  angle_t offsetangle;

  if (m_rw_normalangle > m_rw_angle1) {
    offsetangle = m_rw_normalangle - m_rw_angle1;
  } else {
    offsetangle = m_rw_angle1 - m_rw_normalangle;
  }

  if (offsetangle > ANG90) {
    offsetangle = ANG90;
  }

  const angle_t disAngle = ANG90 - offsetangle;
  const int16_t hyp = PointToDist(seg->line->start->x, seg->line->start->y, player);
  const int16_t sineval = finesine[disAngle >> ANGLE_TO_FINE_SHIFT];
  m_rw_distance = hyp * sineval;

  m_rw_scale = ScaleFromGlobalAngle(player.angle + xtoviewangle[range.start]);

  if (range.end > range.start) {
    const float_t scale2 = ScaleFromGlobalAngle(player.angle + xtoviewangle[range.end]);
    m_rw_scaleStep = (scale2 - m_rw_scale) / (range.end - range.start);
  }

  int32_t wordTop = side->sector->ceilingHeight - player.z;
  int32_t wordBottom = side->sector->floorHeight - player.z;

  int16_t topStep = -(m_rw_scaleStep * wordTop);
  int16_t topFrac = config::CANVAS_HEIGHT / 2 - (m_rw_scaleStep * wordTop);

  int16_t bottomStep = -(m_rw_scaleStep * wordBottom);
  int16_t bottomFrac = config::CANVAS_WIDTH - (m_rw_scaleStep * wordBottom);

  m_ceilplane = CheckVisPlane(m_ceilplane, m_rwx, m_rw_stopx - 1);
  m_floorplane = CheckVisPlane(m_floorplane, m_rwx, m_rw_stopx - 1);

  RenderSegLoop(seg, topStep, topFrac, bottomStep, bottomFrac);
}

void Renderer::RenderSSector(const Player &player, const SubSector &subsector)
{
  int count = subsector.segCount;
  const Sector *frontsector = subsector.sector;
  const seg_t *seg = &m_level->segments[subsector.firstSegIndex];

  if (frontsector->floorHeight < player.z) {
    m_floorplane = FindVisPlane(frontsector->floorHeight, frontsector->color, frontsector->lightLevel);
  } else {
    m_floorplane = nullptr;
  }

  if (frontsector->ceilingHeight > player.z) {
    m_ceilplane = FindVisPlane(frontsector->ceilingHeight, frontsector->color, frontsector->lightLevel);
  } else {
    m_ceilplane = nullptr;
  }

  // render segments
  while (count--) {
    AddSegment(seg, player);
    seg++;
  }
}

void Renderer::RenderBSPNode(const GameState &gameState, const int16_t nodeIndex)
{
  if (nodeIndex & NF_SUBSECTOR) {
    // leaf node
    const SubSector &subsector = m_level->subsectors[nodeIndex & (0x7FFF)];

    RenderSSector(gameState.playerState, subsector);

    return;
  }

  const Node &node = m_level->nodes[nodeIndex];
  const bool side = PointOnSide({ gameState.playerState.x, gameState.playerState.y }, node);

  if (side) {
    RenderBSPNode(gameState, node.leftChild);
  } else {
    RenderBSPNode(gameState, node.rightChild);
  }

  const int16_t farNodeIndex = side ? node.rightChild : node.leftChild;

  // TODO: make a culling method
  RenderBSPNode(gameState, farNodeIndex);
}

int16_t Renderer::ProjectZ(const double_t z, const double_t inv_y) const
{
  return static_cast<int16_t>(static_cast<double_t>(m_canvasHeight) / 2 - z * FOCAL_LENGTH * inv_y);
}

int16_t Renderer::ProjectX(const double_t x, const double_t inv_y) const
{
  return static_cast<int16_t>(static_cast<double>(m_canvasWidth) / 2 + x * FOCAL_LENGTH * inv_y);
}

void Renderer::DrawDefaultPortal(const int32_t x,
                                 const int32_t projectedFloorY,
                                 const int32_t projectedCeilingY,
                                 const int32_t nextFloorY,
                                 const int32_t nextCeilY)
{
  // Render upper wall only if the neighbor ceiling is lower
  if (nextCeilY > projectedCeilingY) {
    const int32_t upperWallBottom = std::max(projectedCeilingY, m_ceilclip[x]);
    const int32_t upperWallTop = std::min(nextCeilY, m_floorclip[x]);

    if (upperWallBottom < upperWallTop) {
      this->DrawColumn(x, upperWallBottom, upperWallTop, MapColor(0, 255, 0, 255));
      m_ceilclip[x] = upperWallTop;
    } else {
      m_ceilclip[x] = upperWallBottom;
    }
  }

  // Render lower wall only when the neighbor floor is higher
  if (nextFloorY < projectedFloorY) {
    const int32_t lowerWallBottom = std::max(nextFloorY, m_ceilclip[x]);// Start at neighbor's floor
    const int32_t lowerWallTop = std::min(projectedFloorY, m_floorclip[x]);

    if (lowerWallBottom < lowerWallTop) {
      this->DrawColumn(x, lowerWallBottom, lowerWallTop, MapColor(0, 255, 0, 255));
      m_floorclip[x] = lowerWallBottom;
    } else {
      m_floorclip[x] = lowerWallTop;
    }
  }
}

// TODO: change color to texture implementation
Visplane *Renderer::FindVisPlane(const int16_t height, const uint32_t color, const int16_t lightLevel)
{
  for (Visplane &vp : m_visplanes) {
    if (vp.height == height && vp.color == color && vp.lightLevel == lightLevel) {
      return &vp;
    }
  }

  m_visplanes.emplace_back(height, color, lightLevel, config::CANVAS_WIDTH, -1);
  Visplane *newPlane = &m_visplanes.back();

  memset(newPlane->top, 0xff, sizeof(newPlane->top));

  return newPlane;
}