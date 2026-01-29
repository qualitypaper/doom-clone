#pragma once

#include <SDL.h>

struct InputState
{
  bool keys[SDL_NUM_SCANCODES];
  bool mouse_buttons[8];
  int mouse_dx;
  int mouse_dy;
};

namespace sdl_window {
class SdlWindow
{
private:
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
public:
  SdlWindow(int16_t width, int16_t height);
  ~SdlWindow();

  int16_t width, height;

  void updatePixels(uint32_t *pixels);
  void updateScreen();

  SDL_Window *getWindow();
  SDL_Renderer *getRenderer();
};
}// namespace sdl_window