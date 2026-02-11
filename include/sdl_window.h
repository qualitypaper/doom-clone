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
  SdlWindow(uint16_t width, uint16_t height);
  ~SdlWindow();

  uint16_t width, height;

  void updatePixels(const uint32_t *pixels);
  void updateScreen() const;

  SDL_Window *getWindow() const;
  SDL_Renderer *getRenderer() const;
};
}// namespace sdl_window