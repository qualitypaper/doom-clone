#include "camera.h"
#include "framebuffer.h"
#include "game_loop.h"

#include <assert.h>
#include <iostream>

static int32_t floorClipping[config::CANVAS_WIDTH];
static int32_t ceilClipping[config::CANVAS_WIDTH];

static int32_t lastCeilingZ[config::CANVAS_WIDTH];
static int32_t lastFloorZ[config::CANVAS_WIDTH];

void poll_sdl_events(InputState &input);
void update(gameloop::GameState &gameState, InputState &input, const double_t dt);
void render(const gameloop::GameState &gameState);
void drawSolidWall(int x, int32_t &projectedCeilingZ, int32_t &projectedFloorZ);
void rotate(float_t x, float_t y, double_t angle, double_t &xRes, double_t &yRes);

constexpr uint32_t mapColor(uint8_t r, uint8_t g, uint8_t b, uint8_t alpha)
{
  return (r << 24) | (g << 16) | (b << 8) | alpha;
}

constexpr uint32_t YELLOW = mapColor(255, 255, 0, 255);

framebuffer::FrameBuffer *fb;
bool running;

// ==========================================
// 1. VERTICES (World Coordinates)
// ==========================================
static const glm::vec2 vertices[] = {
  // Sector 0 (The Starting Room)
  { 0, 0 },// 0
  { 50, 0 },// 1
  { 50, 50 },// 2
  { 0, 50 },// 3

  // Sector 1 (The Connected Hallway - shares 1 and 2 with Sector 0)
  { 100, 0 },// 4
  { 100, 50 }// 5
};

// ==========================================
// 2. SECTORS (Rooms)
// ==========================================
static const gameloop::Sector sectors[] = {

  {
    // Sector 0
    .floorHeight = 0,
    .ceilingHeight = 36,
    .lightLevel = 192,
  },
  {
    // Sector 1 (Taller and deeper)
    .floorHeight = -10,
    .ceilingHeight = 50,
    .lightLevel = 128,
  }
};

// ==========================================
// 3. SIDEDEFS (Visual sides of lines)
// ==========================================
// Note: You usually add textures here. For now, we just link to sectors.
static const gameloop::SideDef sidedefs[] = {
  // -- Sector 0 Sides --
  { .sectorId = 0 },// 0: South wall
  { .sectorId = 0 },// 1: Portal line (facing Sector 1)
  { .sectorId = 0 },// 2: North wall
  { .sectorId = 0 },// 3: West wall

  // -- Sector 1 Sides --
  { .sectorId = 1 },// 4: Portal line (facing Sector 0)
  { .sectorId = 1 },// 5: North wall
  { .sectorId = 1 },// 6: East wall
  { .sectorId = 1 },// 7: South wall
};

// ==========================================
// 4. LINEDEFS (The Geometry)
// ==========================================
// -1 indicates "No Side" (Solid wall)
static const gameloop::LineDef linedefs[] = {
  // --- SECTOR 0 (Square) ---
  // Start, End, Type, FrontSide, BackSide

  // Wall: (0,0) to (50,0)
  { 0, 1, gameloop::LineDefType::REGULAR, 0, -1 },

  // PORTAL: (50,0) to (50,50) -> Connects Sector 0 and 1
  // Notice it has a Back SideDef (index 4)
  { 1, 2, gameloop::LineDefType::REGULAR, 1, 4 },

  // Wall: (50,50) to (0,50)
  { 2, 3, gameloop::LineDefType::REGULAR, 2, -1 },

  // Wall: (0,50) to (0,0)
  { 3, 0, gameloop::LineDefType::REGULAR, 3, -1 },

  // --- SECTOR 1 (Rectangular extension) ---

  // Wall: (50,50) to (100,50)
  { 2, 5, gameloop::LineDefType::REGULAR, 5, -1 },

  // Wall: (100,50) to (100,0)
  { 5, 4, gameloop::LineDefType::REGULAR, 6, -1 },

  // Wall: (100,0) to (50,0)
  { 4, 1, gameloop::LineDefType::REGULAR, 7, -1 },
};

int main()
{
  assert(sectors[0].floorHeight < sectors[0].ceilingHeight);
  for (auto &ld : linedefs) {
    assert(ld.start != ld.end);
    assert(ld.frontSidedef >= 0);
  }

  fb = new framebuffer::FrameBuffer();

  std::cout << "Set up window" << '\n';

  InputState input{};
  gameloop::GameState gameState{};

  gameState.playerState = entity::Player{
    .x = 25, .y = 25, .z = 10, .velocity = 5.0f, .angle = 0, .health = 100, .armor = 100, .current_weapon = 0
  };

  running = true;
  // game loop
  const double dt = 1 / (double)config::DESIRED_FRAMERATE;
  double acc = 0.0;

  uint64_t prev = SDL_GetPerformanceCounter();

  int16_t frameCount = 0;
  double fpsAccumulator = 0.0;

  while (running) {
    for (int i = 0; i < config::CANVAS_WIDTH; ++i) {
      ceilClipping[i] = 0;
      floorClipping[i] = config::CANVAS_HEIGHT;
      lastCeilingZ[i] = 0;
      lastFloorZ[i] = config::CANVAS_HEIGHT;
    }
    fb->reset();
    input.mouse_dx = 0;
    input.mouse_dy = 0;

    poll_sdl_events(input);

    uint64_t now = SDL_GetPerformanceCounter();
    double frameTime = double(now - prev) / SDL_GetPerformanceFrequency();
    prev = now;
    if (frameTime > 0.25) frameTime = 0.25;
    acc += frameTime;

    while (acc >= dt) {
      update(gameState, input, dt);
      acc -= dt;
    }

    render(gameState);

    // FPS tracking
    frameCount++;
    fpsAccumulator += frameTime;
    if (fpsAccumulator >= 1.0) {
      std::printf("FPS: %d\n", frameCount);
      frameCount = 0;
      fpsAccumulator = 0.0;
    }
  }

  if (fb) { delete fb; }

  return 0;
}

void poll_sdl_events(InputState &input)
{
  SDL_Event event;

  while (SDL_PollEvent(&event)) {

    switch (event.type) {
    case SDL_QUIT:
      running = false;
      break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      gameloop::handleKeyInput(event, input);
      break;
    case SDL_MOUSEMOTION:
      gameloop::handleMouseMovement(event, input);
      break;
    }
  }
}

// updates the game state with a constant rate of @param dt
void update(gameloop::GameState &gameState, InputState &input, const double_t dt)
{

  // mouse
  double_t angleDiff = std::atan(config::MOUSE_SENSITIVITY * input.mouse_dx / config::PROJECTION_PLANE_DISTANCE);
  gameState.playerState.angle += angleDiff;

  float_t &x = gameState.playerState.x, &y = gameState.playerState.y;

  float_t sin = std::sin(gameState.playerState.angle);
  float_t cos = std::cos(gameState.playerState.angle);

  int8_t moveSide = 0, moveForward = 0;

  if (input.keys[SDL_SCANCODE_W]) moveForward += 1;
  if (input.keys[SDL_SCANCODE_S]) moveForward -= 1;
  if (input.keys[SDL_SCANCODE_A]) moveSide -= 1;
  if (input.keys[SDL_SCANCODE_D]) moveSide += 1;

  float_t velocity = gameState.playerState.velocity;

  x += (moveSide * cos + moveForward * sin) * dt * velocity;
  y += (moveSide * (-sin) + moveForward * cos) * dt * velocity;
}

void clipNearPlane(double_t &x1, double_t &y1, double_t &x2, double_t &y2)
{
  double_t t = (config::NEAR_CLIPPING - y1) / (y2 - y1);

  x1 += t * (x2 - x1);
  y1 = config::NEAR_CLIPPING;
}

void applyTransformations(const entity::Player &playerState,
  const gameloop::LineDef &ld,
  const gameloop::Sector &sector,
  double_t &viewX1,
  double_t &viewY1,
  double_t &viewX2,
  double_t &viewY2,
  int16_t &floorZ,
  int16_t &ceilingZ)
{
  float_t startX = vertices[ld.start].x - playerState.x;
  float_t startY = vertices[ld.start].y - playerState.y;

  float_t endX = vertices[ld.end].x - playerState.x;
  float_t endY = vertices[ld.end].y - playerState.y;

  floorZ = sector.floorHeight - playerState.z;
  ceilingZ = sector.ceilingHeight - playerState.z;

  rotate(startX, startY, playerState.angle, viewX1, viewY1);
  rotate(endX, endY, playerState.angle, viewX2, viewY2);
}

void rotate(float_t x, float_t y, double_t angle, double_t &xRes, double_t &yRes)
{

  xRes = x * std::cos(angle) - y * std::sin(angle);
  yRes = x * std::sin(angle) + y * std::cos(angle);
}


int32_t clamp(int32_t val, int32_t min, int32_t max) { return std::min(max, std::max(min, val)); }

void render(const gameloop::GameState &gameState)
{
  static const double_t TAN_HALF_FOV = std::tan((config::FOV / 2.0) * (3.14159265 / 180.0));
  static const double_t FOCAL_LENGTH = (120 / 2.0) / TAN_HALF_FOV;
  // const double_t FOCAL_LENGTH = 1;

  for (gameloop::LineDef ld : linedefs) {
    gameloop::SideDef sidedef = sidedefs[ld.frontSidedef];
    gameloop::Sector sector = sectors[sidedef.sectorId];

    double_t viewX1 = 0, viewY1 = 0, viewX2 = 0, viewY2 = 0;
    int16_t floorZ = 0, ceilingZ = 0;

    applyTransformations(gameState.playerState, ld, sector, viewX1, viewY1, viewX2, viewY2, floorZ, ceilingZ);

    // near plane clipping
    if (viewY1 < config::NEAR_CLIPPING && viewY2 < config::NEAR_CLIPPING)
      continue;
    else if (viewY1 < config::NEAR_CLIPPING) {
      clipNearPlane(viewX1, viewY1, viewX2, viewY2);
    } else if (viewY2 < config::NEAR_CLIPPING) {
      clipNearPlane(viewX2, viewY2, viewX1, viewY1);
    }

    int32_t projectedStartX = config::CANVAS_WIDTH / 2 + viewX1 * FOCAL_LENGTH / (viewY1);
    int32_t projectedEndX = config::CANVAS_WIDTH / 2 + viewX2 * FOCAL_LENGTH / (viewY2);

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

    for (int i = std::max(0, start); i < std::min(static_cast<int32_t>(config::CANVAS_WIDTH), end); i++) {
      double_t t = double(i - start) / double(end - start);
      double_t inv_y = lerp(inv_y1, inv_y2, t);

      int32_t projectedFloorZ = config::CANVAS_HEIGHT / 2 - floorZ * FOCAL_LENGTH * inv_y;
      int32_t projectedCeilingZ = config::CANVAS_HEIGHT / 2 - ceilingZ * FOCAL_LENGTH * inv_y;

      if (projectedCeilingZ < 0) { projectedCeilingZ = 0; }

      // draw floor
      if (floorClipping[i] > projectedFloorZ) {
        fb->drawVerticalLine(i, config::CANVAS_HEIGHT - 1, projectedFloorZ, mapColor(255, 255, 0, 255));
        floorClipping[i] = projectedFloorZ;
      }

      if (ceilClipping[i] < projectedCeilingZ) {
        // draw ceiling
        fb->drawVerticalLine(i, 0, projectedCeilingZ, mapColor(255, 0, 0, 255));
        ceilClipping[i] = projectedCeilingZ;
      }

      if (ld.backSidedef == -1) {
        // solid wall
        drawSolidWall(i, projectedCeilingZ, projectedFloorZ);
      } else {
        // portal
        gameloop::SideDef backSidedef = sidedefs[ld.backSidedef];
        gameloop::Sector nextSector = sectors[backSidedef.sectorId];

        int16_t nextCeilZ = nextSector.ceilingHeight - gameState.playerState.z;
        int16_t nextFloorZ = nextSector.floorHeight - gameState.playerState.z;

        int32_t nextCeilY = config::CANVAS_HEIGHT / 2 - nextCeilZ * FOCAL_LENGTH * inv_y;
        int32_t nextFloorY = config::CANVAS_HEIGHT / 2 - nextFloorZ * FOCAL_LENGTH * inv_y;

        // C. Render UPPER Wall (The "step down" from our ceiling to theirs)
        // We only draw if the neighbor's ceiling is lower than ours (physically)
        // or visually lower on screen.
        int32_t upperDrawTop = std::max(projectedCeilingZ, ceilClipping[i]);
        int32_t upperDrawBottom = std::min(nextCeilY, floorClipping[i]);// Stop at neighbor's ceiling

        if (upperDrawTop < upperDrawBottom) {
          fb->drawVerticalLine(i, upperDrawTop, upperDrawBottom, mapColor(0, 255, 0, 255));
          // Important: Update clipping so nothing draws over this upper wall later
          ceilClipping[i] = std::max(ceilClipping[i], upperDrawBottom);
        }

        // D. Render LOWER Wall (The "step up" from our floor to theirs)
        int32_t lowerDrawTop = std::max(nextFloorY, ceilClipping[i]);// Start at neighbor's floor
        int32_t lowerDrawBottom = std::min(projectedFloorZ, floorClipping[i]);

        if (lowerDrawTop < lowerDrawBottom) {
          fb->drawVerticalLine(i, lowerDrawTop, lowerDrawBottom, mapColor(0, 255, 0, 255));
          // Important: Update clipping
          floorClipping[i] = std::min(floorClipping[i], lowerDrawTop);
        }
      }
    }

    // debug
    fb->update();
  }

  fb->update();
}

void drawSolidWall(int x, int32_t &projectedCeilingZ, int32_t &projectedFloorZ)
{

  int32_t drawTop = std::max(projectedCeilingZ, ceilClipping[x]);
  int32_t drawBottom = std::min(projectedFloorZ, floorClipping[x]);

  if (drawTop <= drawBottom) {
    fb->drawVerticalLine(x, drawTop, drawBottom, mapColor(0, 255, 255, 255));

    ceilClipping[x] = drawTop;
    floorClipping[x] = drawBottom;

    lastCeilingZ[x] = projectedCeilingZ;
    lastFloorZ[x] = projectedFloorZ;
  }
}
