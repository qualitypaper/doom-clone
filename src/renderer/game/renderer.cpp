#include "renderer.h"

#include "bsp.h"
#include "config.h"
#include "framebuffer.h"
#include "gameloop.h"
#include "math_utils.h"

#include <algorithm>
#include <fmt/core.h>
#include <utility>

static const double_t TAN_HALF_FOV = std::tan((config::FOV / 2.0) * (M_PI / 180.0));
static double_t FOCAL_LENGTH;

static constexpr uint32_t mapColor(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t alpha)
{
  return (r << 24) | (g << 16) | (b << 8) | alpha;
}

namespace {
template<HasXY T> void clipNearPlane(T &vecToClip, const T &endVec)
{
  const double_t t = (config::NEAR_CLIPPING - vecToClip.y) / (endVec.y - vecToClip.y);

  vecToClip.x += t * (endVec.x - vecToClip.x);
  vecToClip.y = config::NEAR_CLIPPING;
}

template<HasXY T> void rotate(const T &vec, const double_t angle, T &res)
{
  res.x = vec.x * std::cos(angle) - vec.y * std::sin(angle);
  res.y = vec.x * std::sin(angle) + vec.y * std::cos(angle);
}

template<HasXY T, HasXY V>
void applyTransformations(const Player &playerState,
  const V &start,
  const V &end,
  const Sector &floorCeilingSector,
  T &view1,
  T &view2,
  int16_t &floorZ,
  int16_t &ceilingZ)
{
  const double_t startX = start.x - playerState.x;
  const double_t startY = start.y - playerState.y;

  const double_t endX = end.x - playerState.x;
  const double_t endY = end.y - playerState.y;

  floorZ = floorCeilingSector.floorHeight - playerState.z;
  ceilingZ = floorCeilingSector.ceilingHeight - playerState.z;

  rotate(T(startX, startY), playerState.angle, view1);
  rotate(T(endX, endY), playerState.angle, view2);
}
}// namespace

Renderer::Renderer(FrameBuffer &_fb,
  std::shared_ptr<Level> _level,
  const uint16_t canvasWidth,
  const uint16_t canvasHeight)
  : m_fb(_fb), m_level(std::move(_level)), m_canvasWidth(canvasWidth), m_canvasHeight(canvasHeight)
{
  m_ceilingClipping.resize(canvasWidth);
  m_floorClipping.resize(canvasWidth);
  m_visplanes.reserve(MAX_VISPLANES);
  m_solidsegs.resize(MAX_SEGMENTS);

  FOCAL_LENGTH = (canvasWidth / 2.0) / TAN_HALF_FOV;
}

void Renderer::ResetClippingArrays()
{
  std::ranges::fill(m_ceilingClipping, 0);
  std::ranges::fill(m_floorClipping, m_canvasHeight - 1);
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
  const int32_t drawTop = std::max(projectedCeilingZ, m_ceilingClipping[x]);
  const int32_t drawBottom = std::min(projectedFloorZ, m_floorClipping[x]);

  if (drawTop <= drawBottom) {
    DrawColumn(x, drawTop, drawBottom, color);

    m_ceilingClipping[x] = drawBottom;
    m_floorClipping[x] = drawTop;
  }
}

void Renderer::Render(const GameState &gameState)
{
  if (m_level->nodes.empty()) {
    // Entire level is a single subsector (no BSP splits were needed)
    if (!m_level->subsectors.empty()) {
      const SubSector &subsector = m_level->subsectors[0];

      for (int16_t i = subsector.firstSegIndex; i < subsector.firstSegIndex + subsector.segCount; i++) {
        RenderSegment(m_level->segments[i], gameState.playerState);
      }
    }
  } else {
    RenderBSPNode(gameState, 0);
  }

  RenderVisPlanes();

  m_fb.update();
}

bool Renderer::ClipSolidWall(int16_t start, int16_t end, const Seg &seg, const SideDef &sidedef, const Player &player)
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
    return true;
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
        return true;

      while (next++ != m_solidsegs.data() + m_solidsegs.size()) {
        // remove a post
        *(++clipStart) = *next;
      }
    }
  }

  // there is a fragment after next
  StoreWallRange({ static_cast<int16_t>(next->end + 1), end }, seg, sidedef, player);
  next->end = end;
  return false;
}
void Renderer::RenderSegment(const Seg &seg, const Player &player)
{
  const LineDef &ld = m_level->linedefs[seg.linedefIndex];

  const bool isBackSide = seg.side;

  const int16_t frontSidedefIndex = isBackSide ? ld.backSidedef : ld.frontSidedef;
  const int16_t backSidedefIndex = isBackSide ? ld.frontSidedef : ld.backSidedef;

  if (frontSidedefIndex == -1) {
    throw std::runtime_error("frontSidedefIndex == -1 -> The frontSidedef mustn't be empty.");
    return;
  }

  const SideDef &sidedef = m_level->sidedefs[frontSidedefIndex];
  const Sector &sector = m_level->sectors[sidedef.sectorId];

  glm::dvec2 view1 = { 0, 0 }, view2 = { 0, 0 };
  int16_t floorZ = 0, ceilingZ = 0;

  applyTransformations(player,
    m_level->vertices[seg.startVertex],
    m_level->vertices[seg.endVertex],
    sector,
    view1,
    view2,
    floorZ,
    ceilingZ);

  // near plane clipping
  if (view1.y < config::NEAR_CLIPPING && view2.y < config::NEAR_CLIPPING)
    return;

  if (view1.y < config::NEAR_CLIPPING) {
    clipNearPlane(view1, view2);
  } else if (view2.y < config::NEAR_CLIPPING) {
    clipNearPlane(view2, view1);
  }

  const int16_t projectedStartX = ProjectX(view1.x, 1 / view1.y);
  const int16_t projectedEndX = ProjectX(view2.x, 1 / view2.y);

  int16_t start, end;
  double_t inv_y1, inv_y2;

  if (projectedStartX > projectedEndX) {
    start = projectedEndX;
    end = projectedStartX;
    inv_y1 = 1 / view2.y;
    inv_y2 = 1 / view1.y;
  } else {
    start = projectedStartX;
    end = projectedEndX;
    inv_y1 = 1 / view1.y;
    inv_y2 = 1 / view2.y;
  }

  if (start < 0 && end < 0)
    return;

  if (backSidedefIndex == -1) {
    ClipSolidWall(start, end, seg, sidedef, player);
  } else {
  }

  const int16_t loopStart = std::max(static_cast<int16_t>(0), start);
  const int16_t loopEnd = std::min(static_cast<int16_t>(m_canvasWidth - 1), end);

  for (int16_t i = loopStart; i <= loopEnd; i++) {
    const double_t t = static_cast<double>(i - start) / (end - start);
    const double_t inv_y = std::lerp(inv_y1, inv_y2, t);

    // represent ceiling and floor in screen coordinates
    const int16_t projectedFloorY = std::min(static_cast<int16_t>(m_canvasHeight - 1), ProjectZ(floorZ, inv_y));
    const int16_t projectedCeilingY = std::max(static_cast<int16_t>(0), ProjectZ(ceilingZ, inv_y));

    if (backSidedefIndex == -1) {
      // solid wall
      DrawSolidWall(i, projectedCeilingY, projectedFloorY, sector.color);
    } else {
      // portal
      const SideDef backSidedef = m_level->sidedefs[backSidedefIndex];
      const Sector nextSector = m_level->sectors[backSidedef.sectorId];

      const int16_t nextCeilZ = static_cast<int16_t>(static_cast<float>(nextSector.ceilingHeight) - player.z);
      const int16_t nextFloorZ = static_cast<int16_t>(static_cast<float>(nextSector.floorHeight) - player.z);

      // screen Y coordinates
      const int16_t nextCeilY = std::max(static_cast<int16_t>(0), ProjectZ(nextCeilZ, inv_y));
      const int16_t nextFloorY = std::min(static_cast<int16_t>(m_canvasHeight - 1), ProjectZ(nextFloorZ, inv_y));

      if (ld.type == LineDefType::REGULAR) {
        DrawDefaultPortal(i, projectedFloorY, projectedCeilingY, nextFloorY, nextCeilY);
      } else if (ld.type == LineDefType::DOOR) {
        // TODO: create a drawing function for door portal
      }
    }
  }
}

void Renderer::CheckVisPlane() {}

void Renderer::RenderVisPlanes() {}

void Renderer::StoreWallRange(const ClipRange &range, const Seg &seg, const SideDef &sd, const Player &player) {}

void Renderer::RenderSSector(const Player &player, const SubSector &subsector)
{
  int count = subsector.segCount;
  const Sector *frontsector = subsector.sector;
  const Seg *seg = &m_level->segments[subsector.firstSegIndex];

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
    RenderSegment(*seg, player);
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

  const BspNode &node = m_level->nodes[nodeIndex];
  const bool side = PointOnSide(gameState.playerState, node);

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
    const int32_t upperWallBottom = std::max(projectedCeilingY, m_ceilingClipping[x]);
    const int32_t upperWallTop = std::min(nextCeilY, m_floorClipping[x]);

    if (upperWallBottom < upperWallTop) {
      this->DrawColumn(x, upperWallBottom, upperWallTop, mapColor(0, 255, 0, 255));
      m_ceilingClipping[x] = upperWallTop;
    } else {
      m_ceilingClipping[x] = upperWallBottom;
    }
  }

  // Render lower wall only when the neighbor floor is higher
  if (nextFloorY < projectedFloorY) {
    const int32_t lowerWallBottom = std::max(nextFloorY, m_ceilingClipping[x]);// Start at neighbor's floor
    const int32_t lowerWallTop = std::min(projectedFloorY, m_floorClipping[x]);

    if (lowerWallBottom < lowerWallTop) {
      this->DrawColumn(x, lowerWallBottom, lowerWallTop, mapColor(0, 255, 0, 255));
      m_floorClipping[x] = lowerWallBottom;
    } else {
      m_floorClipping[x] = lowerWallTop;
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