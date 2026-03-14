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

template<HasXY T> static void rotate(const T &vec, const double_t angle, T &res)
{
  res.x = vec.x * std::cos(angle) - vec.y * std::sin(angle);
  res.y = vec.x * std::sin(angle) + vec.y * std::cos(angle);
}

template<HasXY T, HasXY V>
void applyTransformations(const entity::Player &playerState,
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
  m_solidSegs.resize(canvasWidth);

  FOCAL_LENGTH = (canvasWidth / 2.0) / TAN_HALF_FOV;
}

void Renderer::ResetClippingArrays()
{
  std::ranges::fill(m_ceilingClipping, 0);
  std::ranges::fill(m_floorClipping, m_canvasHeight - 1);

  for (size_t i = 0; i < m_solidSegs.size(); i++) { m_solidSegs[i] = false; }
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

  if (transformedY0 > transformedY1) std::swap(transformedY0, transformedY1);

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

void Renderer::DrawSolidWall(const int32_t x, const int32_t projectedCeilingZ, const int32_t projectedFloorZ)
{
  const int32_t drawTop = std::max(projectedCeilingZ, m_ceilingClipping[x]);
  const int32_t drawBottom = std::min(projectedFloorZ, m_floorClipping[x]);

  if (drawTop <= drawBottom) {
    this->DrawColumn(x, drawTop, drawBottom, mapColor(0, 255, 255, 255));

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
        RenderSegment(m_level->segments[i], gameState);
      }
    }
  } else {
    RenderBSPNode(gameState, 0);
  }

  m_fb.update();
}

void Renderer::RenderSegment(const Seg &seg, const GameState &gameState)
{
  const LineDef &ld = m_level->linedefs[seg.linedefIndex];

  const bool isBackSide = seg.side;

  if (isBackSide && ld.backSidedef == -1) {
    std::printf("backSidedefIndex == -1 -> When the segment is on the back side, the backSidedef mustn't be empty.");

    fmt::println("Some number");
    return;
  }

  const int16_t frontSidedefId = isBackSide ? ld.backSidedef : ld.frontSidedef;
  const int16_t backSidedefId = isBackSide ? ld.frontSidedef : ld.backSidedef;

  const SideDef &sidedef = m_level->sidedefs[frontSidedefId];
  const Sector &sector = m_level->sectors[sidedef.sectorId];

  glm::dvec2 view1 = { 0, 0 }, view2 = { 0, 0 };
  int16_t floorZ = 0, ceilingZ = 0;

  applyTransformations(gameState.playerState,
    m_level->vertices[seg.startVertex],
    m_level->vertices[seg.endVertex],
    sector,
    view1,
    view2,
    floorZ,
    ceilingZ);

  // near plane clipping
  if (view1.y < config::NEAR_CLIPPING && view2.y < config::NEAR_CLIPPING) return;

  if (view1.y < config::NEAR_CLIPPING) {
    clipNearPlane(view1, view2);
  } else if (view2.y < config::NEAR_CLIPPING) {
    clipNearPlane(view2, view1);
  }

  const int32_t projectedStartX = ProjectX(view1.x, 1 / view1.y);
  const int32_t projectedEndX = ProjectX(view2.x, 1 / view2.y);

  int32_t start, end;
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

  if (start < 0 && end < 0) return;

  for (int32_t i = std::max(0, start); i <= std::min(m_canvasWidth - 1, end); i++) {
    if (m_solidSegs[i]) continue;

    const double_t t = static_cast<double>(i - start) / (end - start);
    const double_t inv_y = std::lerp(inv_y1, inv_y2, t);

    // represent ceiling and floor in screen coordinates
    const int32_t projectedFloorY = std::min(m_canvasHeight - 1, ProjectZ(floorZ, inv_y));
    const int32_t projectedCeilingY = std::max(0, ProjectZ(ceilingZ, inv_y));

    DrawFloor(i, projectedFloorY, sector.color);
    DrawCeiling(i, projectedCeilingY, sector.color);

    if (backSidedefId == -1) {
      // solid wall
      DrawSolidWall(i, projectedCeilingY, projectedFloorY);

      m_solidSegs[i] = true;
    } else {
      // portal
      const SideDef backSidedef = m_level->sidedefs[backSidedefId];
      const Sector nextSector = m_level->sectors[backSidedef.sectorId];

      const int16_t nextCeilZ =
        static_cast<int16_t>(static_cast<float>(nextSector.ceilingHeight) - gameState.playerState.z);
      const int16_t nextFloorZ =
        static_cast<int16_t>(static_cast<float>(nextSector.floorHeight) - gameState.playerState.z);

      // screen Y coordinates
      const int32_t nextCeilY = std::max(0, ProjectZ(nextCeilZ, inv_y));
      const int32_t nextFloorY = std::min(m_canvasHeight - 1, ProjectZ(nextFloorZ, inv_y));

      if (ld.type == LineDefType::REGULAR) {
        DrawDefaultPortal(i, projectedFloorY, projectedCeilingY, nextFloorY, nextCeilY);
      } else if (ld.type == LineDefType::DOOR) {
        // TODO: create a drawing function for door portal
      }

      if (m_ceilingClipping[i] >= m_floorClipping[i]) { m_solidSegs[i] = true; }
    }
  }
}

void Renderer::RenderBSPNode(const GameState &gameState, const int16_t nodeIndex)
{
  if (nodeIndex & 0x8000) {
    // leaf node
    const SubSector &subsector = m_level->subsectors[nodeIndex & 0x7FFF];

    for (int16_t i = subsector.firstSegIndex; i < subsector.firstSegIndex + subsector.segCount; i++) {
      RenderSegment(m_level->segments[i], gameState);
    }

    return;
  }

  const BspNode &node = m_level->nodes[nodeIndex];
  const bool side =
    PointOnSide({ static_cast<int32_t>(gameState.playerState.x), static_cast<int32_t>(gameState.playerState.y) }, node);

  if (side) {
    RenderBSPNode(gameState, node.leftChild);
  } else {
    RenderBSPNode(gameState, node.rightChild);
  }

  const int16_t farNodeIndex = side ? node.rightChild : node.leftChild;

  // TODO: make a culling method
  RenderBSPNode(gameState, farNodeIndex);
}

/**
 *
 * @param v
 * @param node
 * @return true if point is to the left, false when to the right
 */
bool Renderer::PointOnSide(const Vertex v, const BspNode &node)
{
  // Calculate vector from the partition line's origin to the player
  const double_t dx = v.x - node.x;
  const double_t dy = v.y - node.y;

  // 2D Cross Product
  const double_t leftSide = (node.dx * dy) - (node.dy * dx);

  return leftSide > 0;
}


int32_t Renderer::ProjectZ(const double_t z, const double_t inv_y) const
{
  return static_cast<int32_t>(static_cast<double_t>(m_canvasHeight) / 2 - z * FOCAL_LENGTH * inv_y);
}

int32_t Renderer::ProjectX(const double_t x, const double_t inv_y) const
{
  return static_cast<int32_t>(static_cast<double>(m_canvasWidth) / 2 + x * FOCAL_LENGTH * inv_y);
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

void Renderer::DrawCeiling(const int32_t x, const int32_t projectedCeilingY, const uint32_t color = 0xFF00FFFF)
{
  if (m_ceilingClipping[x] <= projectedCeilingY) {
    this->DrawColumn(x, m_ceilingClipping[x], projectedCeilingY, color);
    m_ceilingClipping[x] = projectedCeilingY;
  }
}

void Renderer::DrawFloor(const int32_t x, const int32_t projectedFloorY, const uint32_t color = 0xFF00FF00)
{
  if (m_floorClipping[x] >= projectedFloorY) {
    this->DrawColumn(x, m_floorClipping[x], projectedFloorY, color);
    m_floorClipping[x] = projectedFloorY;
  }
}
