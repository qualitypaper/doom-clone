#include "bsp/bsp.h"
#include "config.h"
#include "core/gameloop.h"
#include "core/simulation.h"
#include "renderer/editor/editor.h"
#include "renderer/editor/editor_renderer.h"
#include "renderer/game/renderer.h"

#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/imgui.h"

#include <SDL.h>
#include <SDL_events.h>

#include <cassert>
#include <fstream>

bool running;
void PollSdlEvents(GameState &gameState, InputState &input, const std::shared_ptr<SdlWindow> &sdlWindow);

// ==========================================
// 1. VERTICES (World Coordinates)
// ==========================================
static std::vector<Vertex> vertices = {
  // Sector 0 (The Starting Room)
  Vertex(0, 0),// 0
  Vertex(200 << FRAC_BITS, 0),// 1
  Vertex(200 << FRAC_BITS, 200 << FRAC_BITS),// 2
  Vertex(0, 200 << FRAC_BITS),// 3

  // Sector 1 (The Connected Hallway - shares 1 and 2 with Sector 0)
  Vertex(500 << FRAC_BITS, 0),// 4
  Vertex(500 << FRAC_BITS, 200 << FRAC_BITS)// 5
};

// ==========================================
// 2. SECTORS (Rooms)
// ==========================================
static std::vector<Sector> sectors = { Sector(0, 300 << FRAC_BITS, -1, -1, 0, 0, 0),
                                       Sector(0, 200 << FRAC_BITS, -1, -1, 0, 0, 0) };

// ==========================================
// 3. SIDEDEFS (Visual sides of lines)
// ==========================================
// Note: You usually add textures here. For now, we just link to sectors.
static std::vector<SideDef> sidedefs = {
  // -- Sector 0 Sides --
  SideDef(0, 0, 0, 0, 0, 0),// 0: South wall
  SideDef(0, 0, 0, 0, 0, 0),// 1: Portal line (facing Sector 1)
  SideDef(0, 0, 0, 0, 0, 0),// 2: North wall
  SideDef(0, 0, 0, 0, 0, 0),// 3: West wall

  // -- Sector 1 Sides --
  SideDef(1, 0, 0, 0, 0, 0),// 4: Portal line (facing Sector 0)
  SideDef(1, 0, 0, 0, 0, 0),// 5: North wall
  SideDef(1, 0, 0, 0, 0, 0),// 6: East wall
  SideDef(1, 0, 0, 0, 0, 0),// 7: South wall
};

// ==========================================
// 4. LINEDEFS (The Geometry)
// ==========================================
// -1 indicates "No Side" (Solid wall)
static std::vector<LineDef> linedefs = {
  // --- SECTOR 0 (Square) ---
  // Start, End, Type, FrontSide, BackSide

  // Wall: (0,0) to (50,0)
  LineDef(0, 1, LineDefType::REGULAR, 0, -1),

  // PORTAL: (50,0) to (50,50) -> Connects Sector 0 and 1
  // Notice it has a Back SideDef (index 4)
  LineDef(1, 2, LineDefType::REGULAR, 1, 4),

  // Wall: (50,50) to (0,50)
  LineDef(2, 3, LineDefType::REGULAR, 2, -1),

  // Wall: (0,50) to (0,0)
  LineDef(3, 0, LineDefType::REGULAR, 3, -1),

  // --- SECTOR 1 (Rectangular extension) ---

  // Wall: (50,50) to (100,50)
  LineDef(2, 5, LineDefType::REGULAR, 5, -1),

  // Wall: (100,50) to (100,0)
  LineDef(5, 4, LineDefType::REGULAR, 6, -1),

  // Wall: (100,0) to (50,0)
  LineDef(4, 1, LineDefType::REGULAR, 7, -1),
};

int main(int argc, char *argv[])
{
  std::shared_ptr<Level> level;
  size_t numOfLevels;

  if (true) {
    std::array<char8_t, 8> levelName = MakeLevelName(1);
    auto tempLevel = std::make_unique<Level>(levelName);
    numOfLevels = EditorLevel::Load(tempLevel);
    level = std::move(tempLevel);
  } else {
    numOfLevels = 1;
    std::array<char8_t, 8> levelName = MakeLevelName(1);
    level = std::make_shared<Level>(levelName);
    level->vertices = std::move(vertices);
    level->sectors = std::move(sectors);
    Level::Init(*level, linedefs, sidedefs, std::vector<Seg>{});
    level->name = MakeLevelName(1);
  }

  // setup sdl window
  auto sdlWindow = std::make_shared<SdlWindow>(
    "Doom Clone", WINDOW_WIDTH, WINDOW_HEIGHT, CANVAS_WIDTH, CANVAS_HEIGHT, SDL_WINDOW_SHOWN);

  // setup Dear ImGui
  auto editorRenderer = std::make_shared<EditorRenderer>(sdlWindow, *level, 1, numOfLevels);

  // setup the game renderer
  auto renderer = std::make_shared<Renderer>(sdlWindow, level);

  GameState gameState{ .player = { .x = 100 << FRAC_BITS,
                                   .y = 0 << FRAC_BITS,
                                   .z = 10 << FRAC_BITS,
                                   .velocity = 30 << FRAC_BITS,
                                   .angle = ANG180 },
                       .currentMode = EngineMode::EDITOR_2D,
                       .levelNum = 1,
                       .sdlWindow = std::move(sdlWindow),
                       .renderer = std::move(renderer),
                       .editorRenderer = std::move(editorRenderer) };


  running = true;
  // game loop
  constexpr double_t dt = 1 / static_cast<double>(DESIRED_FRAMERATE);
  const double_t perfFreq = static_cast<double_t>(SDL_GetPerformanceFrequency());
  double_t acc = 0.0;

  uint64_t prev = SDL_GetPerformanceCounter();

  int16_t frameCount = 0;
  double_t fpsAccumulator = 0.0;

  while (running) {
    const size_t frameStart = SDL_GetPerformanceCounter();
    gameState.Reset();

    PollSdlEvents(gameState, gameState.input, gameState.sdlWindow);

    if (gameState.currentMode == EngineMode::EDITOR_2D) {
      gameState.editorRenderer->Render();
    } else if (gameState.currentMode == EngineMode::BSP_VIEWER) {
      BSPBuilder::Visualize(*gameState.sdlWindow, gameState.input, *level);
    } else {
      // GAMEPLAY_3D
      const uint64_t now = SDL_GetPerformanceCounter();
      double_t frameTime = static_cast<double>(now - prev) / perfFreq;
      prev = now;
      if (frameTime > 0.25)
        frameTime = 0.25;
      acc += frameTime;

      while (acc >= dt) {
        simulation::update(gameState, dt);
        acc -= dt;
      }

      gameState.renderer->Render(gameState.player);

      // FPS tracking
      frameCount++;
      fpsAccumulator += frameTime;
      if (fpsAccumulator >= 1.0) {
        std::printf("FPS: %d\n", frameCount);
        frameCount = 0;
        fpsAccumulator = 0.0;
      }
    }

    const uint64_t frameEnd = SDL_GetPerformanceCounter();
    const double_t elapsed = static_cast<double_t>(frameEnd - frameStart) / perfFreq;
    if (elapsed < dt) {
      SDL_Delay(static_cast<Uint32>((dt - elapsed) * 1000.0));
    }
  }


  return 0;
}

void PollSdlEvents(GameState &gameState, InputState &input, const std::shared_ptr<SdlWindow> &sdlWindow)
{
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL2_ProcessEvent(&event);

    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE
        && event.window.windowID == SDL_GetWindowID(sdlWindow->getWindow())) {
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
          SetEngineMode(gameState, EngineMode::GAMEPLAY_3D, sdlWindow);
        }
        continue;
      } else if (event.key.keysym.sym == SDLK_F2) {
        if (gameState.currentMode != EngineMode::EDITOR_2D) {
          SetEngineMode(gameState, EngineMode::EDITOR_2D, sdlWindow);
        }
      } else if (event.key.keysym.sym == SDLK_F3) {
        if (gameState.currentMode != EngineMode::BSP_VIEWER) {
          SetEngineMode(gameState, EngineMode::BSP_VIEWER, sdlWindow);
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
    default:;
    }
  }
}
