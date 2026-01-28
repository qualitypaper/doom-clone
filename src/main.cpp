#include "framebuffer.h"
#include "gameloop.h"
#include "renderer.h"
#include "simulation.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <SDL.h>

#include <assert.h>
#include <iostream>

static int32_t lastCeilingZ[config::CANVAS_WIDTH];
static int32_t lastFloorZ[config::CANVAS_WIDTH];

void poll_sdl_events(InputState &input);


bool running;

// ==========================================
// 1. VERTICES (World Coordinates)
// ==========================================
static const std::vector<gameloop::Vertex> vertices = {
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
static const std::vector<gameloop::Sector> sectors = {

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
static const std::vector<gameloop::SideDef> sidedefs = {
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
static const std::vector<gameloop::LineDef> linedefs = {
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

  // Initialize SDL window first
  if (!sdl_window::init()) {
    std::cerr << "Failed to initialize SDL window" << '\n';
    return 1;
  }

  framebuffer::FrameBuffer *fb = new framebuffer::FrameBuffer(config::WINDOW_WIDTH, config::WINDOW_HEIGHT);

  renderer::Renderer *renderer = new renderer::Renderer(fb, config::CANVAS_WIDTH, config::CANVAS_HEIGHT);

  InputState input{};
  gameloop::GameState gameState{ .currentMode = gameloop::ViewMode::EDITOR_2D };

  const gameloop::Level level{ vertices, linedefs, sidedefs, sectors };

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

    poll_sdl_events(input);

    if (input.keys[SDL_SCANCODE_F1]) { gameState.currentMode = gameloop::ViewMode::GAMEPLAY_3D; }

    if (gameState.currentMode == gameloop::ViewMode::EDITOR_2D) {
      bool show_demo_window = true;
      bool show_another_window = false;
      ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

      // Start the Dear ImGui frame
      ImGui_ImplSDLRenderer2_NewFrame();
      ImGui_ImplSDL2_NewFrame();
      ImGui::NewFrame();


      // Rendering
      ImGui::Render();
      ImGuiIO &currentIo = ImGui::GetIO();
      SDL_RenderSetScale(
        sdl_window::getRenderer(), currentIo.DisplayFramebufferScale.x, currentIo.DisplayFramebufferScale.y);
      SDL_SetRenderDrawColor(sdl_window::getRenderer(),
        (Uint8)(clear_color.x * 255),
        (Uint8)(clear_color.y * 255),
        (Uint8)(clear_color.z * 255),
        (Uint8)(clear_color.w * 255));
      SDL_RenderClear(sdl_window::getRenderer());
      ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), sdl_window::getRenderer());
      SDL_RenderPresent(sdl_window::getRenderer());

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

  sdl_window::kill();

  return 0;
}

void poll_sdl_events(InputState &input)
{
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL2_ProcessEvent(&event);

    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE
        && event.window.windowID == SDL_GetWindowID(sdl_window::getWindow())) {
      running = false;
      continue;
    } else if (event.type == SDL_QUIT) {
      running = false;
      continue;
    }

    ImGuiIO &io = ImGui::GetIO();

    // Skip game input if ImGui wants to capture, except for F1 (mode toggle)
    if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && io.WantCaptureKeyboard) {
      if (event.key.keysym.sym != SDLK_F1) { continue; }
    }

    if (event.type == SDL_MOUSEMOTION && io.WantCaptureMouse) { continue; }

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