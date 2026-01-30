#include "gameloop.h"
#include <SDL_mouse.h>
#include <SDL_stdinc.h>

struct InputState;

namespace gameloop {

void handleMouseMovement(SDL_Event &event, InputState &input)
{
  input.mouse_dx = event.motion.xrel;
  input.mouse_dy = event.motion.yrel;
}

void handleKeyInput(SDL_Event &event, InputState &input)
{
  bool pressed = (event.type == SDL_KEYDOWN);
  SDL_Scancode scancode = event.key.keysym.scancode;
  input.keys[scancode] = pressed;
}

void setEngineMode(GameState &state, InputState &input, EngineMode newMode)
{
  // nothing to change
  if (state.currentMode == newMode) return;

  state.currentMode = newMode;

  if (newMode == EngineMode::GAMEPLAY_3D) {
    // disable absolute mouse
    // SDL_SetRelativeMouseaaaaaMode(SDL_TRUE);
  } else {
    // in order to use mouse cursor
    SDL_SetRelativeMouseMode(SDL_FALSE);

    // wiping clean the state, in order to prevent unexpected key and mouse inputs
    memset(input.keys, false, sizeof(input.keys));
    memset(input.mouse_buttons, false, sizeof(input.mouse_buttons));
    input.mouse_dx = 0;
    input.mouse_dy = 0;
  }
}
}// namespace gameloop
