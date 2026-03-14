#include "bsp.h"
#include "config.h"
#include "editor.h"
#include "editor_renderer.h"
#include "framebuffer.h"
#include "gameloop.h"
#include "renderer.h"
#include "simulation.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"

#include <SDL.h>
#include <SDL_events.h>

#include <cassert>
#include <fstream>
#include <iostream>

void poll_sdl_events(GameState &gameState, InputState &input, SdlWindow &window);

bool running;

// ==========================================
// 1. VERTICES (World Coordinates)
// ==========================================
static std::vector<Vertex> vertices = {
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
static std::vector<Sector> sectors = {

  {
    // Sector 0
    .floorHeight = 0,
    .ceilingHeight = 36,
  },
  {
    // Sector 1 (Taller and deeper)
    .floorHeight = 0,
    .ceilingHeight = 20,
  }
};

// ==========================================
// 3. SIDEDEFS (Visual sides of lines)
// ==========================================
// Note: You usually add textures here. For now, we just link to sectors.
static std::vector<SideDef> sidedefs = {
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
static std::vector<LineDef> linedefs = {
  // --- SECTOR 0 (Square) ---
  // Start, End, Type, FrontSide, BackSide

  // Wall: (0,0) to (50,0)
  { 0, 1, LineDefType::REGULAR, 0, -1 },

  // PORTAL: (50,0) to (50,50) -> Connects Sector 0 and 1
  // Notice it has a Back SideDef (index 4)
  { 1, 2, LineDefType::REGULAR, 1, 4 },

  // Wall: (50,50) to (0,50)
  { 2, 3, LineDefType::REGULAR, 2, -1 },

  // Wall: (0,50) to (0,0)
  { 3, 0, LineDefType::REGULAR, 3, -1 },

  // --- SECTOR 1 (Rectangular extension) ---

  // Wall: (50,50) to (100,50)
  { 2, 5, LineDefType::REGULAR, 5, -1 },

  // Wall: (100,50) to (100,0)
  { 5, 4, LineDefType::REGULAR, 6, -1 },

  // Wall: (100,0) to (50,0)
  { 4, 1, LineDefType::REGULAR, 7, -1 },
};

int main()
{
  // setup inputs and states
  InputState input{};
  GameState gameState{ .currentMode = EngineMode::EDITOR_2D };

  gameState.playerState = entity::Player{
    .x = 0, .y = 0, .z = 10, .velocity = 15.0f, .angle = 0, .health = 100, .armor = 100, .current_weapon = 0
  };

  std::shared_ptr<Level> level;
  if (true) {
    EditorLevel edLevel{};
    edLevel.deserialize("saved_level.bin");
    Level tempLevel;
    edLevel.toGameLevel(tempLevel, config::EDITOR_WINDOW_WIDTH, config::EDITOR_WINDOW_HEIGHT);
    level = std::make_shared<Level>(tempLevel);
  } else {
    level = std::make_shared<Level>(vertices, linedefs, sidedefs, sectors);
  }

  BSPBuilder bspBuilder(*level);
  bspBuilder.BuildBSPTree();
  bspBuilder.PrintTree();

  level = std::make_shared<Level>(*bspBuilder.GetConstructedLevel());

  // setup sdl window
  SdlWindow sdlWindow("Doom Clone",
    gameState.currentMode == EngineMode::EDITOR_2D ? config::EDITOR_WINDOW_WIDTH : config::WINDOW_WIDTH,
    gameState.currentMode == EngineMode::EDITOR_2D ? config::EDITOR_WINDOW_HEIGHT : config::WINDOW_HEIGHT,
    gameState.currentMode == EngineMode::EDITOR_2D ? SDL_WINDOW_SHOWN : SDL_WINDOW_SHOWN);

  // setup Dear ImGui
  EditorRenderer imguiRenderer(sdlWindow, *level);

  // setup the game renderer
  FrameBuffer fb(sdlWindow);
  Renderer renderer(fb, level, config::CANVAS_WIDTH, config::CANVAS_HEIGHT);

  running = true;
  // game loop
  constexpr double_t dt = 1 / static_cast<double>(config::DESIRED_FRAMERATE);
  const double_t perfFreq = static_cast<double_t>(SDL_GetPerformanceFrequency());
  double_t acc = 0.0;

  uint64_t prev = SDL_GetPerformanceCounter();

  int16_t frameCount = 0;
  double_t fpsAccumulator = 0.0;

  while (running) {
    const uint64_t frameStart = SDL_GetPerformanceCounter();
    // reseting the states to defaults
    renderer.ResetClippingArrays();
    input.mouse_dx = 0;
    input.mouse_dy = 0;

    poll_sdl_events(gameState, input, sdlWindow);

    if (gameState.currentMode == EngineMode::EDITOR_2D) {
      imguiRenderer.render();

      const uint64_t frameEnd = SDL_GetPerformanceCounter();
      const double_t elapsed = static_cast<double_t>(frameEnd - frameStart) / perfFreq;
      if (elapsed < dt) { SDL_Delay(static_cast<Uint32>((dt - elapsed) * 1000.0)); }
      continue;
    } else if (gameState.currentMode == EngineMode::BSP_VIEWER) {
      bspBuilder.Visualize(sdlWindow, input);

      const uint64_t frameEnd = SDL_GetPerformanceCounter();
      const double_t elapsed = static_cast<double_t>(frameEnd - frameStart) / perfFreq;
      if (elapsed < dt) { SDL_Delay(static_cast<Uint32>((dt - elapsed) * 1000.0)); }
      continue;
    }

    uint64_t now = SDL_GetPerformanceCounter();
    double_t frameTime = static_cast<double>(now - prev) / perfFreq;
    prev = now;
    if (frameTime > 0.25) frameTime = 0.25;
    acc += frameTime;

    while (acc >= dt) {
      simulation::update(gameState, input, dt);
      acc -= dt;
    }

    renderer.Render(gameState);

    // FPS tracking
    frameCount++;
    fpsAccumulator += frameTime;
    if (fpsAccumulator >= 1.0) {
      std::printf("FPS: %d\n", frameCount);
      frameCount = 0;
      fpsAccumulator = 0.0;
    }

    const uint64_t frameEnd = SDL_GetPerformanceCounter();
    const double_t elapsed = static_cast<double_t>(frameEnd - frameStart) / perfFreq;
    if (elapsed < dt) { SDL_Delay(static_cast<Uint32>((dt - elapsed) * 1000.0)); }
  }


  return 0;
}

void poll_sdl_events(GameState &gameState, InputState &input, SdlWindow &window)
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

    // early handle of the mode change key
    if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.sym == SDLK_F1) {
        if (gameState.currentMode != EngineMode::GAMEPLAY_3D) {
          SetEngineMode(gameState, input, EngineMode::GAMEPLAY_3D, window);
        }


        continue;
      } else if (event.key.keysym.sym == SDLK_F2) {
        if (gameState.currentMode != EngineMode::EDITOR_2D) {
          SetEngineMode(gameState, input, EngineMode::EDITOR_2D, window);
        }
      } else if (event.key.keysym.sym == SDLK_F3) {
        if (gameState.currentMode != EngineMode::BSP_VIEWER) {
          SetEngineMode(gameState, input, EngineMode::BSP_VIEWER, window);
        }
      }
    }

    // early skip for preventing capturing mouse and keyboard inputs, while in EDITOR_2D engine mode
    if (gameState.currentMode == EngineMode::EDITOR_2D && ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP))) {
      continue;
    }

    switch (event.type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      HandleKeyInput(event, input);
      break;
    case SDL_MOUSEMOTION:
      HandleMouseMovement(event, input);
      break;
    }
  }
}
