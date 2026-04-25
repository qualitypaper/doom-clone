#include "bsp/bsp.h"
#include "config.h"
#include "core/gameloop.h"
#include "core/serialization/colors_serializer.h"
#include "core/serialization/texture_serializer.h"
#include "core/simulation.h"
#include "renderer/editor/editor.h"
#include "renderer/editor/editor_input_handler.h"
#include "renderer/game/renderer.h"

#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/imgui.h"

#include "log.h"

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

void HandleModeChange(GameState &gameState, const std::shared_ptr<SdlWindow> &sdlWindow)
{
  if (gameState.input.keys[SDL_SCANCODE_F1]) {
    if (gameState.currentMode != EngineMode::GAMEPLAY_3D) {
      SetEngineMode(gameState, EngineMode::GAMEPLAY_3D, sdlWindow);
    }
  } else if (gameState.input.keys[SDL_SCANCODE_F2]) {
    if (gameState.currentMode != EngineMode::EDITOR_2D) {
      SetEngineMode(gameState, EngineMode::EDITOR_2D, sdlWindow);
    }
  } else if (gameState.input.keys[SDL_SCANCODE_F3]) {
    if (gameState.currentMode != EngineMode::BSP_VIEWER) {
      SetEngineMode(gameState, EngineMode::BSP_VIEWER, sdlWindow);
    }
  }
}

int main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  Log::Init();
  DOOM_CORE_INFO("Logger initialized");

  std::shared_ptr<Level> level;
  size_t numOfLevels;

  if (1) {
    std::array<char8_t, 8> levelName = Level::MakeLevelName(1);
    auto tempLevel = std::make_unique<Level>(levelName);
    numOfLevels = EditorLevel::Load(tempLevel);
    level = std::move(tempLevel);
  } else {
    numOfLevels = 1;
    std::array<char8_t, 8> levelName = Level::MakeLevelName(1);
    level = std::make_shared<Level>(levelName);
    level->vertices = std::move(vertices);
    level->sectors = std::move(sectors);
    Level::Init(*level, linedefs, sidedefs, std::vector<Seg>{});
    level->name = levelName;
  }

  // setup sdl window
  auto sdlWindow = std::make_shared<SdlWindow>(
    "Doom Clone", 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, SDL_WINDOW_RESIZABLE);


  std::unique_ptr<PaletteManager> paletteManager;
  std::unique_ptr<TextureManager> textureManager;
  // initialize palette/textures
  {
    paletteManager = std::make_unique<PaletteManager>(PROJECT_ROOT_PATH "PLAYPAL.pal");

    std::vector<FlatTexture> flatTextures = FlatTexture::ReadAll();
    std::vector<PatchView> wallTextures = PatchView::ReadAll();
    // TODO: load wall textures

    textureManager =
      std::make_unique<TextureManager>(std::move(flatTextures), std::move(wallTextures), paletteManager.get());
  }

  auto editor = std::make_shared<Editor>(sdlWindow, *level, 1, numOfLevels, *textureManager, *paletteManager);

  // setup the game renderer
  auto renderer = std::make_shared<Renderer>(sdlWindow, level);


  GameState gameState{ .player = { .x = 0 << FRAC_BITS,
                                   .y = 0 << FRAC_BITS,
                                   .z = 10 << FRAC_BITS,
                                   .velocity = 30 << FRAC_BITS,
                                   .angle = ANG180 },
                       .currentMode = EngineMode::EDITOR_2D,
                       .levelNum = 1,
                       .sdlWindow = std::move(sdlWindow),
                       .renderer = std::move(renderer),
                       .editor = std::move(editor),
                       .texManager =  std::move(textureManager),
            .palManager = std::move(paletteManager)

  };


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

    gameState.sdlWindow->PollEvents(gameState.input, running);

    // early handle of the mode change key
    HandleModeChange(gameState, gameState.sdlWindow);

    if (gameState.currentMode == EngineMode::EDITOR_2D) {
      gameState.editor->Render();
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

      gameState.renderer->Render(gameState);

      // FPS tracking
      frameCount++;
      fpsAccumulator += frameTime;
      if (fpsAccumulator >= 1.0) {
        DOOM_INFO("FPS: {}", frameCount);
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
