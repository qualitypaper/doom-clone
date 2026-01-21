#include "camera.h"
#include "framebuffer.h"
#include "game_loop.h"

#include <assert.h>
#include <iostream>

static int32_t floorClipping[config::CANVAS_WIDTH];
static int32_t ceilClipping[config::CANVAS_WIDTH];

void poll_sdl_events(InputState &input);
void update(const gameloop::GameState &gameState);
void render(const gameloop::GameState &gameState, InputState &input);

constexpr uint32_t mapColor(uint8_t r, uint8_t g, uint8_t b, uint8_t alpha)
{
  return (r << 24) | (g << 16) | (b << 8) | alpha;
}

constexpr uint32_t YELLOW = mapColor(255, 255, 0, 255);

framebuffer::FrameBuffer *fb;
bool running;

static const glm::vec2 vertices[] = {
  { 0, 0 },
  { 100, 0 },
  { 100, 100 },
  { 0, 100 },
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

  // SDL_SetRelativeMouseMode(SDL_TRUE);

  std::cout << "Set up window" << '\n';

  InputState input{};
  gameloop::GameState gameState{};

  gameState.playerState = entity::Player{
    .x = 50, .y = 50, .z = 10, .velocity = 1.0f, .angle = 0, .health = 100, .armor = 100, .current_weapon = 0
  };

  for (int i = 0; i < config::CANVAS_WIDTH; ++i) {
    ceilClipping[i] = 0;
    floorClipping[i] = config::CANVAS_HEIGHT;
  }

  running = true;
  // game loop
  time_t timestamp = time(0);

  std::cout << "Start timestamp: " << timestamp << '\n';

  while (running) {
    input.mouse_dx = 0;
    input.mouse_dy = 0;

    poll_sdl_events(input);

    update(gameState);
    render(gameState, input);
    system("sleep 1");

    if (time(0) - timestamp > 5000) { running = false; }
  }

  if (fb) { delete fb; }

  return 0;
}

void poll_sdl_events(InputState &input)
{
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // std::cout << "Event type: " << event.type << '\n';

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
  std::printf("Finished polling events \n");
}

void update(const gameloop::GameState &gameState) {}

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

  int16_t startX = vertices[ld.start].x - playerState.x;
  int16_t startY = vertices[ld.start].y - playerState.y;

  int16_t endX = vertices[ld.end].x - playerState.x;
  int16_t endY = vertices[ld.end].y - playerState.y;

  floorZ = sector.floorHeight - playerState.z;
  ceilingZ = sector.ceilingHeight - playerState.z;

  viewX1 = startX * std::cos(playerState.angle) - startY * std::sin(playerState.angle);
  viewY1 = startX * std::sin(playerState.angle) + startY * std::cos(playerState.angle);
  viewX2 = endX * std::cos(playerState.angle) - endY * std::sin(playerState.angle);
  viewY2 = endX * std::sin(playerState.angle) + endY * std::cos(playerState.angle);
}


int32_t clamp(int32_t val, int32_t min, int32_t max) {
  return std::min(max, std::max(min, val));
}

void render(const gameloop::GameState &gameState, InputState &input)
{

  // for (int i = 0; i < 50; i++)
  // {
  //     fb->drawHorizontalLine(i, 50, 100, YELLOW);
  // }
  // fb->drawVerticalLine(50, 50, 100, YELLOW);

  // fb->update();

  constexpr double_t FOV = 90.0;
  const double_t TAN_HALF_FOV = std::tan((FOV / 2.0) * (3.14159265 / 180.0));
  const double_t FOCAL_LENGTH = (20 / 2.0) / TAN_HALF_FOV;
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

    int32_t projectedStartX = clamp((int32_t) config::CANVAS_WIDTH / 2 + viewX1 * FOCAL_LENGTH / (viewY1), 0, config::CANVAS_WIDTH);
    int32_t projectedEndX = clamp((int32_t) config::CANVAS_WIDTH / 2 + viewX2 * FOCAL_LENGTH / (viewY2), 0, config::CANVAS_WIDTH);

    int16_t start, end;
    double_t inv_y1, inv_y2;

    if (projectedStartX > projectedEndX) {
      start = projectedEndX;
      end = projectedStartX;
      inv_y1 = 1 / viewY2;
      inv_y2 = 1 / viewY1;
    } else {
      start = projectedStartX;
      end = projectedEndX;
      inv_y1 = 1 / (double)viewY1;
      inv_y2 = 1 / (double)viewY2;
    }

    transformedY0 = std::max(0, transformedY0);
transformedY1 = std::min(config::WINDOW_HEIGHT - 1, transformedY1);

    for (int i = start; i < end; i++) {
      double_t t = double(i - start) / double(end - start);
      double_t inv_y = lerp(inv_y1, inv_y2, t);

      int32_t projectedFloorZ = config::CANVAS_HEIGHT / 2 - floorZ * FOCAL_LENGTH * inv_y;
      int32_t projectedCeilingZ = config::CANVAS_HEIGHT / 2 - ceilingZ * FOCAL_LENGTH * inv_y;

      ceilClipping[i] = std::max(ceilClipping[i], projectedFloorZ + 1);
      floorClipping[i] = std::min(floorClipping[i], projectedCeilingZ - 1);

      if (projectedCeilingZ < 0) { projectedCeilingZ = 0; }

      fb->drawVerticalLine(i, projectedFloorZ, projectedCeilingZ, mapColor(0, 255, std::rand(), 255));
    }
  }

  fb->update();
}