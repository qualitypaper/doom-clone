#include "framebuffer.h"
#include "gameloop.h"
#include "imgui_renderer.h"
#include "math_utils.h"
#include "renderer.h"
#include "simulation.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <SDL.h>

#include <SDL_events.h>
#include <assert.h>
#include <iostream>

void poll_sdl_events(gameloop::GameState &gameState, InputState &input, sdl_window::SdlWindow &window);
constexpr ImVec2 toCenterCoordinates(ImVec2 vec, sdl_window::SdlWindow &sdlWindow);
constexpr ImVec2 convertVertexIntoImVec2(gameloop::Vertex vertex);

bool running;

// ==========================================
// 1. VERTICES (World Coordinates)
// ==========================================
static std::vector<gameloop::Vertex> vertices = {
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
static std::vector<gameloop::Sector> sectors = {

  { // Sector 0
    .floorHeight = 0,
    .ceilingHeight = 36,
    .specialType = 0,
    .lightLevel = 192,
    .tag = 0 },
  { // Sector 1 (Taller and deeper)
    .floorHeight = -10,
    .ceilingHeight = 50,
    .specialType = 0,
    .lightLevel = 128,
    .tag = 0 }
};

// ==========================================
// 3. SIDEDEFS (Visual sides of lines)
// ==========================================
// Note: You usually add textures here. For now, we just link to sectors.
static std::vector<gameloop::SideDef> sidedefs = {
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
static std::vector<gameloop::LineDef> linedefs = {
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
  // sanity checks for hardcoded values
  for (auto &sector : sectors) { assert(sector.floorHeight < sector.ceilingHeight); }
  for (auto &ld : linedefs) {
    assert(ld.start != ld.end);
    assert(ld.frontSidedef >= 0);
  }

  sdl_window::SdlWindow *sdlWindow = new sdl_window::SdlWindow(config::WINDOW_WIDTH, config::WINDOW_HEIGHT);

  // setup Dear ImGui
  imguirenderer::ImguiRenderer *imguiRenderer = new imguirenderer::ImguiRenderer(*sdlWindow);

  // setup the game renderer
  framebuffer::FrameBuffer *fb = new framebuffer::FrameBuffer(*sdlWindow);
  renderer::Renderer *renderer = new renderer::Renderer(fb, config::CANVAS_WIDTH, config::CANVAS_HEIGHT);

  InputState input{};
  gameloop::GameState gameState{ .currentMode = gameloop::EngineMode::EDITOR_2D };

  gameloop::Level level{ vertices, linedefs, sidedefs, sectors };

  gameState.playerState = entity::Player{
    .x = 25, .y = 25, .z = 10, .velocity = 5.0f, .angle = 0, .health = 100, .armor = 100, .current_weapon = 0
  };

  running = true;
  // game loop
  const double_t dt = 1 / static_cast<double>(config::DESIRED_FRAMERATE);
  double_t acc = 0.0;

  uint64_t prev = SDL_GetPerformanceCounter();

  int16_t frameCount = 0;
  double_t fpsAccumulator = 0.0;

  while (running) {
    // reseting the states to defaults
    renderer->resetClippingArrays();
    fb->reset();
    input.mouse_dx = 0;
    input.mouse_dy = 0;

    poll_sdl_events(gameState, input, *sdlWindow);

    if (gameState.currentMode == gameloop::EngineMode::EDITOR_2D) {
      std::vector<ImVec4> scaledLinedefs;

      for (int i = 0; i < linedefs.size(); i++) {
        auto &ld = linedefs[i];

        ImVec2 start = toCenterCoordinates(convertVertexIntoImVec2(level.vertices[ld.start]), *sdlWindow);
        ImVec2 end = toCenterCoordinates(convertVertexIntoImVec2(level.vertices[ld.end]), *sdlWindow);

        scaledLinedefs.push_back(ImVec4(start.x, start.y, end.x, end.y));
      }

      imguiRenderer->render(level, scaledLinedefs);

      continue;
    }

    uint64_t now = SDL_GetPerformanceCounter();
    double_t frameTime = (double)(now - prev) / SDL_GetPerformanceFrequency();
    prev = now;
    if (frameTime > 0.25) frameTime = 0.25;
    acc += frameTime;

    while (acc >= dt) {
      simulation::update(gameState, input, dt);
      acc -= dt;
    }

    renderer->render(gameState, level);

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
  if (renderer) { delete renderer; }
  if (sdlWindow) { delete sdlWindow; }
  if (imguiRenderer) { delete imguiRenderer; }

  return 0;
}

constexpr ImVec2 toCenterCoordinates(ImVec2 vec, sdl_window::SdlWindow &sdlWindow)
{
  return ImVec2(std::max(0.0f, std::min(static_cast<float>(sdlWindow.width), sdlWindow.width / 2 + vec.x)),
    std::max(0.0f, std::min(static_cast<float>(sdlWindow.height), sdlWindow.height / 2 - vec.y)));
}

constexpr ImVec2 convertVertexIntoImVec2(gameloop::Vertex vertex) { return ImVec2(vertex.y, vertex.x); }

void poll_sdl_events(gameloop::GameState &gameState, InputState &input, sdl_window::SdlWindow &window)
{
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL2_ProcessEvent(&event);

    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE
        && event.window.windowID == SDL_GetWindowID(window.getWindow())) {
      running = false;
      continue;
    } else if (event.type == SDL_QUIT) {
      running = false;
      continue;
    }

    ImGuiIO &io = ImGui::GetIO();

    // early handle of the mode change key
    if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.sym == SDLK_F1) {
        std::cout << "Changing mode\n";
        if (gameState.currentMode == gameloop::EngineMode::GAMEPLAY_3D) {
          gameloop::setEngineMode(gameState, input, gameloop::EngineMode::EDITOR_2D);
        } else {
          gameloop::setEngineMode(gameState, input, gameloop::EngineMode::GAMEPLAY_3D);
        }

        continue;
      }
    }

    // early skip for preventing capturing mouse and keyboard inputs, while in EDITOR_2D engine mode
    if (gameState.currentMode == gameloop::EngineMode::EDITOR_2D
        && ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP))) {
      continue;
    }

    switch (event.type) {
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