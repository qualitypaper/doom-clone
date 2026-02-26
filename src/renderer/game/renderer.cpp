#include "renderer.h"

#include <utility>

#include "bsp.h"
#include "config.h"
#include "framebuffer.h"
#include "gameloop.h"

#include <algorithm>

static const double_t TAN_HALF_FOV = std::tan((config::FOV / 2.0) * (3.14159265 / 180.0));
static double_t FOCAL_LENGTH;

static constexpr uint32_t mapColor(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t alpha)
{ return (r << 24) | (g << 16) | (b << 8) | alpha; }

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

    m_ceilingClipping[x] = drawTop;
    m_floorClipping[x] = drawBottom;
  }
}

void clipNearPlane(double_t &x1, double_t &y1, const double_t x2, const double_t y2)
{
  const double_t t = (config::NEAR_CLIPPING - y1) / (y2 - y1);

  x1 += t * (x2 - x1);
  y1 = config::NEAR_CLIPPING;
}

void rotate(const float_t x, const float_t y, const double_t angle, double_t &xRes, double_t &yRes)
{
  xRes = x * std::cos(angle) - y * std::sin(angle);
  yRes = x * std::sin(angle) + y * std::cos(angle);
}

void applyTransformations(const entity::Player &playerState,
  const Vertex start,
  const Vertex end,
  const Sector &sector,
  double_t &viewX1,
  double_t &viewY1,
  double_t &viewX2,
  double_t &viewY2,
  int16_t &floorZ,
  int16_t &ceilingZ)
{
  const float_t startX = start.x - playerState.x;
  const float_t startY = start.y - playerState.y;

  const float_t endX = end.x - playerState.x;
  const float_t endY = end.y - playerState.y;

  floorZ = sector.floorHeight - playerState.z;
  ceilingZ = sector.ceilingHeight - playerState.z;

  rotate(startX, startY, playerState.angle, viewX1, viewY1);
  rotate(endX, endY, playerState.angle, viewX2, viewY2);
}


void Renderer::Render(const GameState &gameState)
{
  RenderBSPNode(gameState, 0);

  m_fb.update();
}

void Renderer::RenderSegment(const Seg &seg, const GameState &gameState)
{
  const LineDef &ld = m_level->linedefs[seg.linedefIndex];

  const SideDef &sidedef = m_level->sidedefs[ld.frontSidedef];
  const Sector &sector = m_level->sectors[sidedef.sectorId];

  double_t viewX1 = 0, viewY1 = 0, viewX2 = 0, viewY2 = 0;
  int16_t floorZ = 0, ceilingZ = 0;

  applyTransformations(gameState.playerState,
    m_level->vertices[ld.start],
    m_level->vertices[ld.end],
    sector,
    viewX1,
    viewY1,
    viewX2,
    viewY2,
    floorZ,
    ceilingZ);

  // near plane clipping
  if (viewY1 < config::NEAR_CLIPPING && viewY2 < config::NEAR_CLIPPING) return;

  if (viewY1 < config::NEAR_CLIPPING) {
    clipNearPlane(viewX1, viewY1, viewX2, viewY2);
  } else if (viewY2 < config::NEAR_CLIPPING) {
    clipNearPlane(viewX2, viewY2, viewX1, viewY1);
  }

  const int32_t projectedStartX = ProjectX(viewX1, 1 / viewY1);
  const int32_t projectedEndX = ProjectX(viewX2, 1 / viewY2);

  int32_t start, end;
  double_t inv_y1, inv_y2;

  if (projectedStartX > projectedEndX) {
    start = projectedEndX;
    end = projectedStartX;
    inv_y1 = 1 / viewY2;
    inv_y2 = 1 / viewY1;
  } else {
    start = projectedStartX;
    end = projectedEndX;
    inv_y1 = 1 / viewY1;
    inv_y2 = 1 / viewY2;
  }

  if (start < 0 && end < 0) return;

  int realStart = 0, realEnd = m_canvasWidth - 1;
  {
    int startTemp = std::ranges::clamp(start, 0, static_cast<int32_t>(m_canvasWidth - 1));
    int endTemp = std::ranges::clamp(end, 0, static_cast<int32_t>(m_canvasWidth - 1));

    while (startTemp < m_solidSegs.size() && m_solidSegs[startTemp]) { startTemp++; }

    while (endTemp >= 0 && m_solidSegs[endTemp]) { endTemp--; }

    if (startTemp >= m_solidSegs.size() || endTemp < 0) return;
    realStart = startTemp;
    realEnd = endTemp;
  }

  for (int32_t i = std::max(realStart, start); i < std::min(realEnd, end); i++) {
    const double_t t = static_cast<double>(i - start) / (end - start);
    const double_t inv_y = lerp(inv_y1, inv_y2, t);

    // represent ceiling and floor in screen coordinates
    const int32_t projectedFloorY = ProjectZ(floorZ, inv_y);
    const int32_t projectedCeilingY = std::max(0, ProjectZ(ceilingZ, inv_y));

    DrawFloor(i, projectedFloorY, sidedef.color);
    DrawCeiling(i, projectedCeilingY, sidedef.color);

    if (ld.backSidedef == -1) {
      // solid wall
      this->DrawSolidWall(i, projectedCeilingY, projectedFloorY);
      for (int j = realStart; j <= realEnd; j++) { m_solidSegs[j] = true; }
    } else {
      // portal
      const SideDef backSidedef = m_level->sidedefs[ld.backSidedef];
      const Sector nextSector = m_level->sectors[backSidedef.sectorId];

      const int16_t nextCeilZ =
        static_cast<int16_t>(static_cast<float>(nextSector.ceilingHeight) - gameState.playerState.z);
      const int16_t nextFloorZ =
        static_cast<int16_t>(static_cast<float>(nextSector.floorHeight) - gameState.playerState.z);

      // screen Y coordinates
      const int32_t nextCeilY = ProjectZ(nextCeilZ, inv_y);
      const int32_t nextFloorY = ProjectZ(nextFloorZ, inv_y);

      if (ld.type == LineDefType::REGULAR) {
        DrawDefaultPortal(i, projectedFloorY, projectedCeilingY, nextFloorY, nextCeilY);
      } else if (ld.type == LineDefType::DOOR) {
        // TODO: create a drawing function for door portal
      }
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
  // checks whether a point is located in the leftBoundingBox of the node
  return node.leftBoundingBox[0] > v.y && node.leftBoundingBox[2] < v.y && node.leftBoundingBox[1] > v.x
         && node.leftBoundingBox[3] < v.x;
}


int32_t Renderer::ProjectZ(const double_t z, const double_t inv_y) const
{ return static_cast<int32_t>(static_cast<double_t>(m_canvasHeight) / 2 - z * FOCAL_LENGTH * inv_y); }

int32_t Renderer::ProjectX(const double_t x, const double_t inv_y) const
{ return static_cast<int32_t>(static_cast<double>(m_canvasWidth) / 2 + x * FOCAL_LENGTH * inv_y); }

void Renderer::DrawDefaultPortal(const int32_t x,
  const int32_t projectedFloorY,
  const int32_t projectedCeilingY,
  const int32_t nextFloorY,
  const int32_t nextCeilY)
{
  // Render upper wall only if the neighbor ceiling is lower
  if (nextCeilY > projectedCeilingY) {

    const int32_t upperWallTop = std::max(projectedCeilingY, m_ceilingClipping[x]);
    const int32_t upperWallBottom = std::min(nextCeilY, m_floorClipping[x]);

    if (upperWallTop < upperWallBottom) {
      this->DrawColumn(x, upperWallTop, upperWallBottom, mapColor(0, 255, 0, 255));
      m_ceilingClipping[x] = upperWallBottom;
    } else {
      m_ceilingClipping[x] = upperWallTop;
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
