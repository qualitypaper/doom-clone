#include "renderer.h"

#include "config.h"
#include "math_utils.h"
#include "renderer_helper.h"
#include "tables.h"
#include "utils.h"

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
      t = config::CANVAS_WIDTH;
    } else {
      // normal case
      t = FixedMul(FOCAL_LENGTH, finetangent[i]);
      // rounding up to the nearest
      t = (config::CANVAS_CENTERX_FRAC - t + FRAC_UNIT - 1) >> FRAC_BITS;

      if (t < -1) {
        t = -1;
      } else if (t > config::CANVAS_WIDTH + 1) {
        t = config::CANVAS_WIDTH;
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


// m_rwDistance must be calculated first
fixed_t Renderer::ScaleFromGlobalAngle(const angle_t angle, const Player &player) const
{
  const angle_t anglea = ANG90 + (angle - player.angle);
  const angle_t angleb = ANG90 + (angle - m_rw_normalAngle);

  // both sines are allways positive
  const fixed_t sinea = finesine[anglea >> ANGLE_TO_FINE_SHIFT];
  const fixed_t sineb = finesine[angleb >> ANGLE_TO_FINE_SHIFT];
  const fixed_t num = FixedMul(config::CANVAS_CENTERX_FRAC, sineb);
  const fixed_t den = FixedMul(m_rw_distance, sinea);

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

Renderer::Renderer(FrameBuffer &_fb,
                   std::shared_ptr<Level> _level,
                   const uint16_t canvasWidth,
                   const uint16_t canvasHeight)
  : m_fb(_fb), m_level(std::move(_level)), m_canvasWidth(canvasWidth), m_canvasHeight(canvasHeight)
{
  m_ceilClip.resize(canvasWidth);
  m_floorClip.resize(canvasWidth);
  m_visplanes.reserve(MAX_VISPLANES);
  m_solidSegs.resize(MAX_SEGMENTS);
  ResetSolidSegs();

  InitViewAngleToX();
  InitXToViewAngle();
}

void Renderer::ResetClippingArrays()
{
  std::ranges::fill(m_ceilClip, 0);
  std::ranges::fill(m_floorClip, m_canvasHeight - 1);
  m_visplanes.clear();
  ResetSolidSegs();
}

void Renderer::ResetSolidSegs()
{
  m_solidSegs[0].start = -0x7fffffff;
  m_solidSegs[0].end = -1;
  m_solidSegs[1].start = m_canvasWidth;
  m_solidSegs[1].end = 0x7fffffff;
  m_newEnd = m_solidSegs.data() + 2;
}

void Renderer::DrawColumn(const int x, const int y0, const int y1, const uint32_t color) const
{
  // scale the point to the window size
  int scaledX = static_cast<int>(config::SCALE_X * x);
  int transformedY0 = static_cast<int>(config::SCALE_Y * y0);
  int transformedY1 = static_cast<int>(config::SCALE_Y * y1);

  scaledX = std::clamp(scaledX, 0, config::WINDOW_WIDTH - 1);
  transformedY0 = std::clamp(transformedY0, 0, config::WINDOW_HEIGHT - 1);
  transformedY1 = std::clamp(transformedY1, 0, config::WINDOW_HEIGHT - 1);

  if (transformedY0 > transformedY1)
    std::swap(transformedY0, transformedY1);

  // using window width as the pitch, because the current
  // implementation doesn't leave any extra pixels
  const uint32_t pitch = this->m_fb.width;

  for (uint32_t i = 0; i < config::SCALE_X + 1; i++) {
    uint32_t *ptr = this->m_fb.pixels + transformedY0 * pitch + (scaledX + i);

    for (int32_t y = transformedY0; y <= transformedY1; y++) {
      *(uint32_t *)ptr = color;
      ptr += pitch;
    }
  }
}


void Renderer::ClipSolidWall(int start, int end, const seg_t *seg, const side_t *sidedef, const Player &player)
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
      StoreWallRange({ start, end }, seg, sidedef, player);

      next = m_newEnd;
      m_newEnd++;

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
    StoreWallRange({ start, static_cast<int16_t>(clipStart->start - 1) }, seg, sidedef, player);
    clipStart->start = start;
  }

  // bottom contained in start
  if (end <= clipStart->end) {
    return;
  }

  next = clipStart;

  while (next + 1 - m_solidSegs.data() < m_solidSegs.size() && end >= (next + 1)->start - 1) {
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

      while (clipStart < m_solidSegs.data() + m_solidSegs.size() && next++ != m_solidSegs.data() + m_solidSegs.size()) {
        // remove a post
        *(++clipStart) = *next;
      }

      return;
    }
  }

  // there is a fragment after next
  StoreWallRange({ static_cast<int16_t>(next->end + 1), end }, seg, sidedef, player);
  next->end = end;
}

void Renderer::ClipPassWall(int start, int end, const seg_t *seg, const side_t *side, const Player &player)
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

void Renderer::RenderSeg(const seg_t *seg, const Player &player)
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

  angle1 = (angle1 + FOV) >> ANGLE_TO_FINE_SHIFT;
  angle2 = (angle2 + FOV) >> ANGLE_TO_FINE_SHIFT;

  const int x1 = m_viewAngleToX[angle1];
  const int x2 = m_viewAngleToX[angle2];

  const bool isBackSide = seg->side;

  const side_t *frontSide = isBackSide ? ld.backSide : ld.frontSide;
  const side_t *backSide = isBackSide ? ld.frontSide : ld.backSide;

  if (!frontSide) {
    throw std::runtime_error("frontSidedefIndex == -1 -> The frontSidedef mustn't be empty/null.");
  }

  if (!backSide) {
    ClipSolidWall(x1, x2, seg, frontSide, player);
  } else {
    ClipPassWall(x1, x2, seg, backSide, player);
  }
}

Visplane *Renderer::CheckVisPlane(Visplane *visplane, const int start, const int end)
{
  if (!visplane)
    return nullptr;

  if (end <= start)
    return visplane;

  int internalLow, internalHigh;
  int unionLow, unionHigh;

  if (start < visplane->minX) {
    internalLow = visplane->minX;
    unionLow = start;
  } else {
    internalLow = start;
    unionLow = visplane->minX;
  }

  if (end > visplane->maxX) {
    internalHigh = visplane->maxX;
    unionHigh = end;
  } else {
    unionHigh = visplane->maxX;
    internalHigh = end;
  }

  int x;

  // look for changed y's
  for (x = internalLow; x <= internalHigh; x++) {
    if (visplane->top[x] != -1) {
      break;
    }
  }

  if (x > internalHigh) {
    // use the same one
    visplane->minX = unionLow;
    visplane->maxX = unionHigh;

    return visplane;
  }

  // make a new visplane
  Visplane &lastVisplane = m_visplanes.back();
  lastVisplane.height = visplane->height;
  lastVisplane.lightLevel = visplane->lightLevel;
  lastVisplane.color = visplane->color;


  visplane = &m_visplanes.emplace_back();
  visplane->minX = start;
  visplane->maxX = end;

  memset(visplane->top, -1, sizeof(visplane->top));

  return visplane;
}

void Renderer::RenderVisPlanes() {}

void Renderer::RenderSegLoop(const seg_t *seg,
                             const fixed_t topStep,
                             fixed_t topFrac,
                             const fixed_t bottomStep,
                             fixed_t bottomFrac)
{
  for (int x = m_rwx; x < std::min(m_rwStopX, (int)config::CANVAS_WIDTH); x++) {
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
    }

    topFrac += topStep;
    bottomFrac += bottomStep;
  }
}

void Renderer::StoreWallRange(const ClipRange &range, const seg_t *seg, const side_t *side, const Player &player)
{
  // production version, commented out for testing
#if 1
  // calculate rw_distance for scale calculation
  m_rw_normalAngle = seg->angle + ANG90;
  angle_t offsetangle;

  if (m_rw_normalAngle > m_rw_angle1) {
    offsetangle = m_rw_normalAngle - m_rw_angle1;
  } else {
    offsetangle = m_rw_angle1 - m_rw_normalAngle;
  }

  if (offsetangle > ANG90) {
    offsetangle = ANG90;
  }

  const angle_t disAngle = ANG90 - offsetangle;
  const fixed_t hyp = PointToDist(seg->line->start->x, seg->line->start->y, player);
  const fixed_t sineval = finesine[disAngle >> ANGLE_TO_FINE_SHIFT];
  m_rw_distance = FixedMul(hyp, sineval);

  m_rwx = range.start;
  m_rwStopX = range.end + 1;

  m_rw_scale = ScaleFromGlobalAngle(player.angle + m_xToViewAngle[range.start], player);

  if (range.end > range.start) {
    const fixed_t scale2 = ScaleFromGlobalAngle(player.angle + m_xToViewAngle[range.end], player);
    m_rw_scaleStep = (scale2 - m_rw_scale) / (range.end - range.start);
  } else {
    m_rw_scaleStep = 0;
  }

  fixed_t wordTop = side->sector->ceilingHeight - player.z;
  fixed_t wordBottom = side->sector->floorHeight - player.z;

  fixed_t topStep = -FixedMul(m_rw_scaleStep, wordTop);
  fixed_t topFrac = (config::CANVAS_CENTERY_FRAC >> 4) - FixedMul(m_rw_scale, wordTop);

  fixed_t bottomStep = -FixedMul(m_rw_scaleStep, wordBottom);
  fixed_t bottomFrac = (config::CANVAS_CENTERY_FRAC >> 4) - FixedMul(m_rw_scale, wordBottom);

  m_ceilPlane = CheckVisPlane(m_ceilPlane, m_rwx, m_rwStopX - 1);
  m_floorPlane = CheckVisPlane(m_floorPlane, m_rwx, m_rwStopX - 1);

  RenderSegLoop(seg, topStep, topFrac, bottomStep, bottomFrac);

#endif

#if 0
  // use the initial version
  // project every coord with the standard 1/y
  double wordTop = FixedToDouble(side->sector->ceilingHeight - player.z);
  double wordBottom = FixedToDouble(side->sector->floorHeight - player.z);

  // transform y coord
  const double y1 = FixedToDouble(seg->start->y - player.y);
  const double y2 = FixedToDouble(seg->end->y - player.y);

  const double x1 = FixedToDouble(seg->start->x - player.x);
  const double x2 = FixedToDouble(seg->end->x - player.x);

  // rotate both coordinates by player.angle
  double playerAngleSin = FixedToDouble(finesine[player.angle >> ANGLE_TO_FINE_SHIFT]);
  double playerAngleCos = FixedToDouble(finesine[(player.angle + ANG90) >> ANGLE_TO_FINE_SHIFT]);
  double rotatedX1 = x1 * playerAngleCos - y1 * playerAngleSin;
  double rotatedY1 = x1 * playerAngleSin + y1 * playerAngleCos;

  double rotatedX2 = x2 * playerAngleCos - y2 * playerAngleSin;
  double rotatedY2 = x2 * playerAngleSin + y2 * playerAngleCos;

  double invY1 = 1 / rotatedY1;
  double invY2 = 1 / rotatedY2;

  int x1Proj = ProjectX(rotatedX1, invY1);
  int x2Proj = ProjectX(rotatedX2, invY2);

  if (x2Proj < x1Proj) {
    std::swap(x1Proj, x2Proj);
    std::swap(invY1, invY2);
  }

  for (int x = std::max(0, x1Proj); x <= std::min(config::CANVAS_WIDTH - 1, x2Proj); x++) {
    const double t = (x - x1Proj) / static_cast<double>(x2Proj - x1Proj);
    const double invY = std::lerp(invY1, invY2, t);

    const int yTop = ProjectZ(wordTop, invY);
    const int yBottom = ProjectZ(wordBottom, invY);

    DrawColumn(x, yTop, yBottom, seg->line->frontSide->sector->color);
  }
#endif
}

void Renderer::RenderSSector(const Player &player, const SubSector &subsector)
{
  int count = subsector.segCount;
  const Sector *frontsector = subsector.sector;
  const seg_t *seg = &m_level->segments[subsector.firstSegIndex];

  if (frontsector->floorHeight < player.z) {
    m_floorPlane = FindVisPlane(frontsector->floorHeight, frontsector->color, frontsector->lightLevel);
  } else {
    m_floorPlane = nullptr;
  }

  if (frontsector->ceilingHeight > player.z) {
    m_ceilPlane = FindVisPlane(frontsector->ceilingHeight, frontsector->color, frontsector->lightLevel);
  } else {
    m_ceilPlane = nullptr;
  }

  // render segments
  while (count--) {
    RenderSeg(seg, player);
    seg++;
  }
}

void Renderer::RenderBSPNode(const GameState &gameState, const int16_t nodeIndex)
{
  if (nodeIndex & NF_SUBSECTOR) {
    // leaf node
    const SubSector &subsector = m_level->subsectors[nodeIndex & static_cast<uint16_t>(~NF_SUBSECTOR)];

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

int Renderer::ProjectZ(const double_t z, const double_t inv_y) const
{
  return static_cast<int>(static_cast<double_t>(m_canvasHeight) / 2 - z * FOCAL_LENGTH * inv_y);
}

int Renderer::ProjectX(const double_t x, const double_t inv_y) const
{
  return static_cast<int>(static_cast<double>(m_canvasWidth) / 2 + x * FOCAL_LENGTH * inv_y);
}

// TODO: change color to texture implementation
Visplane *Renderer::FindVisPlane(const fixed_t height, const uint32_t color, const int16_t lightLevel)
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

void Renderer::Render(const GameState &gameState)
{
  ResetClippingArrays();
  // production version, turned off to test other parts
#if 0
  if (m_level->nodes.empty()) {
    // Entire level is a single subsector (no BSP splits were needed)
    if (!m_level->subsectors.empty()) {
      const SubSector &subsector = m_level->subsectors[0];

      for (int16_t i = subsector.firstSegIndex; i < subsector.firstSegIndex + subsector.segCount; i++) {
        RenderSeg(m_level->segments.data() + i, gameState.playerState);
      }
    }
  } else {
    RenderBSPNode(gameState, 0);
  }
#endif

  // for testing just render every segment
  for (line_t &line : m_level->linedefs) {
    seg_t seg{
      line.start, line.end, PointToAngle2(line.start->x, line.start->y, line.end->x, line.end->y), &line, 0, 0
    };

    RenderSeg(&seg, gameState.playerState);

    if (line.backSide) {
      seg_t backSeg{ line.start, line.end, PointToAngle2(line.start->x, line.start->y, line.end->x, line.end->y),
                     &line,      1,        0 };

      RenderSeg(&backSeg, gameState.playerState);
    }
  }

  RenderVisPlanes();

  m_fb.update();
}
