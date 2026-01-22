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

constexpr uint32_t mapColor(uint8_t r, uint8_t g, uint8_t b, uint8_t alpha)
{
  return (r << 24) | (g << 16) | (b << 8) | alpha;
}

constexpr uint32_t YELLOW = mapColor(255, 255, 0, 255);

framebuffer::FrameBuffer *fb;
bool running;

static const glm::vec2 vertices[] = {
  { 0, 0 },
  { 50, 0 },
  { 50, 50 },
  { 0, 50 },
  // {0, -256},
  // {-256, -256},
  // {-256, 0},
};

static const gameloop::Sector sectors[] = { {
  .floorHeight = 0,
  .ceilingHeight = 36,
  .lightLevel = 192,
} };

static const gameloop::SideDef sidedefs[] = {
  { 0, 0, 0 },
  { 0, 0, 0 },
  { 0, 0, 0 },
  { 0, 0, 0 },
};

static const gameloop::LineDef linedefs[] = {
  // { 0, 1, gameloop::LineDefType::REGULAR, 0, -1 },
  { 1, 2, gameloop::LineDefType::REGULAR, 1, -1 },
  { 2, 3, gameloop::LineDefType::REGULAR, 2, -1 },
  { 3, 0, gameloop::LineDefType::REGULAR, 3, -1 },
  // {4, 5, gameloop::LineDefType::REGULAR, 3, -1},
  // {5, 6, gameloop::LineDefType::REGULAR, 3, -1},
  // {6, 0, gameloop::LineDefType::REGULAR, 3, -1},
  // {0, 4, gameloop::LineDefType::REGULAR, 3, -1},
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
    .x = 25, .y = 25, .z = 10, .velocity = 2.0f, .angle = 0, .health = 100, .armor = 100, .current_weapon = 0
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

void update(gameloop::GameState &gameState, InputState &input, const double_t dt)
{
  // update the movement of the player

  // keyboard
  float_t &x = gameState.playerState.x, &y = gameState.playerState.y;

  for (uint32_t i = 0; i < sizeof(input.keys); i++) {
    if (!input.keys[i]) continue;

    SDL_Scancode scancode = static_cast<SDL_Scancode>(i);
    SDL_Keycode keycode = SDL_GetKeyFromScancode(scancode);
    const char *sym = SDL_GetKeyName(keycode);
    const char lower = tolower(sym[0]);

    if (lower == 'w') {
      y += dt * gameState.playerState.velocity;
    } else if (lower == 's') {
      y -= dt * gameState.playerState.velocity;
    } else if (lower == 'a') {
      x -= dt * gameState.playerState.velocity;
    } else if (lower == 'd') {
      x += dt * gameState.playerState.velocity;
    }
  }
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

  viewX1 = startX * std::cos(playerState.angle) - startY * std::sin(playerState.angle);
  viewY1 = startX * std::sin(playerState.angle) + startY * std::cos(playerState.angle);
  viewX2 = endX * std::cos(playerState.angle) - endY * std::sin(playerState.angle);
  viewY2 = endX * std::sin(playerState.angle) + endY * std::cos(playerState.angle);
}


int32_t clamp(int32_t val, int32_t min, int32_t max) { return std::min(max, std::max(min, val)); }

void render(const gameloop::GameState &gameState)
{
  const double_t TAN_HALF_FOV = std::tan((config::FOV / 2.0) * (3.14159265 / 180.0));
  const double_t FOCAL_LENGTH = (120 / 2.0) / TAN_HALF_FOV;
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

      int32_t drawTop = std::max(projectedCeilingZ, ceilClipping[i]);
      int32_t drawBottom = std::min(projectedFloorZ, floorClipping[i]);

      if (drawTop <= drawBottom) {
        fb->drawVerticalLine(i, drawTop, drawBottom, mapColor(0, 255, 255, 255));

        ceilClipping[i] = std::max(ceilClipping[i], projectedCeilingZ);
        floorClipping[i] = std::min(floorClipping[i], projectedFloorZ);

        lastCeilingZ[i] = projectedCeilingZ;
        lastFloorZ[i] = projectedFloorZ;
      }

      fb->drawVerticalLine(i, 0, ceilClipping[i], mapColor(255, 0, 0, 255));
      fb->drawVerticalLine(i, config::CANVAS_HEIGHT - 1, floorClipping[i], mapColor(255, 255, 0, 255));
    }
  }

  fb->update();
}