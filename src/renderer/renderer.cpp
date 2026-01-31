#include "renderer.h"
#include "framebuffer.h"
#include "gameloop.h"

namespace renderer {
static const double_t TAN_HALF_FOV = std::tan((config::FOV / 2.0) * (3.14159265 / 180.0));
static double_t FOCAL_LENGTH;

static constexpr uint32_t mapColor(uint8_t r, uint8_t g, uint8_t b, uint8_t alpha)
{
  return (r << 24) | (g << 16) | (b << 8) | alpha;
}

Renderer::Renderer(framebuffer::FrameBuffer *fb, uint16_t canvasWidth, uint16_t canvasHeight)
  : fb(fb), canvasWidth(canvasWidth), canvasHeight(canvasHeight)
{
  ceilingClipping.resize(canvasWidth);
  floorClipping.resize(canvasWidth);

  FOCAL_LENGTH = (canvasWidth / 2.0) / TAN_HALF_FOV;
}

void Renderer::resetClippingArrays()
{
  std::fill(ceilingClipping.begin(), ceilingClipping.end(), 0);
  std::fill(floorClipping.begin(), floorClipping.end(), this->canvasHeight - 1);
}

void Renderer::drawSolidWall(int x, int32_t &projectedCeilingZ, int32_t &projectedFloorZ)
{
  int32_t drawTop = std::max(projectedCeilingZ, ceilingClipping[x]);
  int32_t drawBottom = std::min(projectedFloorZ, floorClipping[x]);

  if (drawTop <= drawBottom) {
    this->drawColumn(x, drawTop, drawBottom, mapColor(0, 255, 255, 255));

    ceilingClipping[x] = drawTop;
    floorClipping[x] = drawBottom;
  }
}

void Renderer::drawColumn(int16_t x, int16_t y0, int16_t y1, uint32_t color)
{
  // scale the point to the window size
  int32_t scaledX = config::SCALE_X * x;
  int32_t transformedY0 = config::SCALE_Y * y0;
  int32_t transformedY1 = config::SCALE_Y * y1;

  if (scaledX < 0 || scaledX > config::WINDOW_WIDTH - 1 || transformedY0 < 0
      || transformedY0 > config::WINDOW_HEIGHT - 1 || transformedY1 < 0 || transformedY1 > config::WINDOW_HEIGHT - 1) {
    return;
  }

  if (transformedY0 > transformedY1) std::swap(transformedY0, transformedY1);

  // using window width as the pitch, because the current
  // implementation doesn't leave any extra pixels
  uint32_t pitch = this->fb->width;

  transformedY0 = std::max(0, transformedY0);
  transformedY1 = std::min(this->fb->height - 1, transformedY1);

  for (uint32_t i = 0; i < config::SCALE_X + 1; i++) {
    uint32_t *ptr = this->fb->pixels + (transformedY0)*pitch + (scaledX + i);

    for (int32_t y = transformedY0; y <= transformedY1; y++) {
      *(uint32_t *)ptr = color;
      ptr += pitch;
    }
  }
}

void clipNearPlane(double_t &x1, double_t &y1, double_t &x2, double_t &y2)
{
  double_t t = (config::NEAR_CLIPPING - y1) / (y2 - y1);

  x1 += t * (x2 - x1);
  y1 = config::NEAR_CLIPPING;
}

void rotate(float_t x, float_t y, double_t angle, double_t &xRes, double_t &yRes)
{

  xRes = x * std::cos(angle) - y * std::sin(angle);
  yRes = x * std::sin(angle) + y * std::cos(angle);
}

void applyTransformations(const entity::Player &playerState,
  const gameloop::Vertex start,
  const gameloop::Vertex end,
  const gameloop::Sector &sector,
  double_t &viewX1,
  double_t &viewY1,
  double_t &viewX2,
  double_t &viewY2,
  int16_t &floorZ,
  int16_t &ceilingZ)
{
  float_t startX = start.x - playerState.x;
  float_t startY = start.y - playerState.y;

  float_t endX = end.x - playerState.x;
  float_t endY = end.y - playerState.y;

  floorZ = sector.floorHeight - playerState.z;
  ceilingZ = sector.ceilingHeight - playerState.z;

  rotate(startX, startY, playerState.angle, viewX1, viewY1);
  rotate(endX, endY, playerState.angle, viewX2, viewY2);
}


void Renderer::render(const gameloop::GameState &gameState, const gameloop::Level &level)
{

  for (gameloop::LineDef ld : level.linedefs) {
    gameloop::SideDef sidedef = level.sidedefs[ld.frontSidedef];
    gameloop::Sector sector = level.sectors[sidedef.sectorId];

    double_t viewX1 = 0, viewY1 = 0, viewX2 = 0, viewY2 = 0;
    int16_t floorZ = 0, ceilingZ = 0;

    applyTransformations(gameState.playerState,
      level.vertices[ld.start],
      level.vertices[ld.end],
      sector,
      viewX1,
      viewY1,
      viewX2,
      viewY2,
      floorZ,
      ceilingZ);

    // near plane clipping
    if (viewY1 < config::NEAR_CLIPPING && viewY2 < config::NEAR_CLIPPING)
      continue;
    else if (viewY1 < config::NEAR_CLIPPING) {
      clipNearPlane(viewX1, viewY1, viewX2, viewY2);
    } else if (viewY2 < config::NEAR_CLIPPING) {
      clipNearPlane(viewX2, viewY2, viewX1, viewY1);
    }

    int32_t projectedStartX = projectX(viewX1, 1 / viewY1);
    int32_t projectedEndX = projectX(viewX2, 1 / viewY2);

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

    for (int32_t i = std::max(0, start); i < std::min(static_cast<int32_t>(canvasWidth), end); i++) {
      double_t t = static_cast<double>(i - start) / (end - start);
      double_t inv_y = lerp(inv_y1, inv_y2, t);

      // represent ceiling and floor in screen coordinates
      int32_t projectedFloorY = projectZ(floorZ, inv_y);
      int32_t projectedCeilingY = projectZ(ceilingZ, inv_y);

      if (projectedCeilingY < 0) { projectedCeilingY = 0; }

      drawFloor(i, projectedFloorY);
      drawCeiling(i, projectedCeilingY);

      if (ld.backSidedef == -1) {
        // solid wall
        this->drawSolidWall(i, projectedCeilingY, projectedFloorY);
      } else {
        // portal
        gameloop::SideDef backSidedef = level.sidedefs[ld.backSidedef];
        gameloop::Sector nextSector = level.sectors[backSidedef.sectorId];

        int16_t nextCeilZ = nextSector.ceilingHeight - gameState.playerState.z;
        int16_t nextFloorZ = nextSector.floorHeight - gameState.playerState.z;

        // screen Y coordinates
        int32_t nextCeilY = projectZ(nextCeilZ, inv_y);
        int32_t nextFloorY = projectZ(nextFloorZ, inv_y);

        if (ld.type == gameloop::LineDefType::REGULAR) {
          drawDefaultPortal(i, projectedFloorY, projectedCeilingY, nextFloorY, nextCeilY);
        } else if (ld.type == gameloop::LineDefType::DOOR) {
          // TODO: create a drawing function for door portal
        }
      }
    }
  }

  fb->update();
}

int32_t Renderer::projectZ(int32_t z, double_t inv_y) { return canvasHeight / 2 - z * FOCAL_LENGTH * inv_y; }

int32_t Renderer::projectX(int32_t x, double_t inv_y) { return canvasWidth / 2 + x * FOCAL_LENGTH * inv_y; }

void Renderer::drawDefaultPortal(int32_t x,
  int32_t projectedFloorY,
  int32_t projectedCeilingY,
  int32_t nextFloorY,
  int32_t nextCeilY)
{
  // Render upper wall only if the neighbour ceiling is lower
  if (nextCeilY > projectedCeilingY) {

    int32_t upperWallTop = std::max(projectedCeilingY, ceilingClipping[x]);
    int32_t upperWallBottom = std::min(nextCeilY, floorClipping[x]);

    if (upperWallTop < upperWallBottom) {
      this->drawColumn(x, upperWallTop, upperWallBottom, mapColor(0, 255, 0, 255));
      ceilingClipping[x] = upperWallBottom;
    } else {
      ceilingClipping[x] = upperWallTop;
    }
  }

  // Render lower wall only when the neighbor floor is higher
  if (nextFloorY < projectedFloorY) {
    int32_t lowerWallBottom = std::max(nextFloorY, ceilingClipping[x]);// Start at neighbor's floor
    int32_t lowerWallTop = std::min(projectedFloorY, floorClipping[x]);

    if (lowerWallBottom < lowerWallTop) {
      this->drawColumn(x, lowerWallBottom, lowerWallTop, mapColor(0, 255, 0, 255));
      floorClipping[x] = lowerWallBottom;
    } else {
      floorClipping[x] = lowerWallTop;
    }
  }
}

void Renderer::drawCeiling(int32_t x, int32_t projectedCeilingY)
{
  if (ceilingClipping[x] <= projectedCeilingY) {
    this->drawColumn(x, ceilingClipping[x], projectedCeilingY, mapColor(255, 0, 0, 255));
    ceilingClipping[x] = projectedCeilingY;
  }
}

void Renderer::drawFloor(int32_t x, int32_t projectedFloorY)
{
  if (floorClipping[x] >= projectedFloorY) {
    this->drawColumn(x, floorClipping[x], projectedFloorY, mapColor(255, 200, 0, 255));
    floorClipping[x] = projectedFloorY;
  }
}


}// namespace renderer
