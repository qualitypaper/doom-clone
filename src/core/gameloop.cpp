#include "gameloop.h"
#include "bsp.h"
#include "config.h"
#include "sdl_window.h"

#include <SDL_mouse.h>
#include <SDL_stdinc.h>
#include <SDL_video.h>

struct InputState;

void HandleMouseMovement(const SDL_Event &event, InputState &input)
{
  input.mouse_dx = event.motion.xrel;
  input.mouse_dy = event.motion.yrel;
}

void HandleKeyInput(const SDL_Event &event, InputState &input)
{
  const bool pressed = (event.type == SDL_KEYDOWN);
  const SDL_Scancode scancode = event.key.keysym.scancode;
  input.keys[scancode] = pressed;
}

void SetEngineMode(GameState &state, InputState &input, const EngineMode newMode, const SdlWindow &sdlWindow)
{
  // nothing to change
  if (state.currentMode == newMode)
    return;

  state.currentMode = newMode;

  if (newMode == EngineMode::GAMEPLAY_3D) {
    // disable absolute mouse
    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_SetWindowFullscreen(sdlWindow.getWindow(), SDL_FALSE);
    SDL_SetWindowPosition(sdlWindow.getWindow(), SDL_WINDOWPOS_CENTERED_DISPLAY(1), SDL_WINDOWPOS_CENTERED_DISPLAY(1));
    SDL_SetWindowSize(sdlWindow.getWindow(), config::WINDOW_WIDTH, config::WINDOW_HEIGHT);
  } else {
    // in order to use mouse cursor
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_SetWindowSize(sdlWindow.getWindow(), config::EDITOR_WINDOW_WIDTH, config::EDITOR_WINDOW_HEIGHT);
    SDL_SetWindowFullscreen(sdlWindow.getWindow(), SDL_TRUE);

    // wiping clean the state, in order to prevent unexpected key and mouse inputs
    memset(input.keys, false, sizeof(input.keys));
    memset(input.mouse_buttons, false, sizeof(input.mouse_buttons));
    input.mouse_dx = 0;
    input.mouse_dy = 0;
  }
}

void Level::InitializeBspParams(std::unique_ptr<BspLevel> bspLevel)
{
  this->nodes = std::move(bspLevel->nodes);
  this->segments = std::move(bspLevel->segments);
  this->subsectors = std::move(bspLevel->subsectors);

  if (this->vertices.size() != bspLevel->vertices.size()) {
    this->vertices = std::move(bspLevel->vertices);
  }

  if (this->linedefs.size() != bspLevel->linedefs.size()) {
    this->linedefs = std::move(bspLevel->linedefs);
  }
}

void Level::Load(FileReader &fr)
{
  if (!fr.IsStreamGood()) {
    throw std::runtime_error("Failed to open file for loading.");
  }

  fr.ReadVector(this->linedefs);
  fr.ReadVector(this->sidedefs);
  fr.ReadVector(this->vertices);
  fr.ReadVector(this->segments);
  fr.ReadVector(this->subsectors);
  fr.ReadVector(this->nodes);
  fr.ReadVector(this->sectors);
}
