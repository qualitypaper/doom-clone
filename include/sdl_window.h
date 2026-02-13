#pragma once

#include <SDL.h>

struct InputState
{
  bool keys[SDL_NUM_SCANCODES];
  bool mouse_buttons[8];
  int mouse_dx;
  int mouse_dy;
};

class SdlWindow
{
private:
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
public:
  SdlWindow(uint16_t width, uint16_t height, uint32_t flags = 0);
  ~SdlWindow();

  uint16_t width, height;

  void updatePixels(const uint32_t *pixels) const;
  void updateScreen() const;

  [[nodiscard]] SDL_Window *getWindow() const;
  [[nodiscard]] SDL_Renderer *getRenderer() const;
};