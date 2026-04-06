#pragma once

#include <SDL.h>
#include <stdexcept>

struct InputState
{
  bool keys[SDL_NUM_SCANCODES];
  bool mouse_buttons[8];
  int mouse_dx;
  int mouse_dy;
};

class SdlWindow
{
public:
  SdlWindow(const char *title,
            uint16_t windowWidth, // size of the actual window
            uint16_t windowHeight,
            uint16_t renderWidth,// internal render resolution
            uint16_t renderHeight,
            uint32_t windowFlags = 0);// e.g. SDL_WINDOW_RESIZABLE

  ~SdlWindow();

  // Update the streaming texture with your rendered pixels (RGBA8888)
  void updatePixels(const uint32_t *pixels) const;

  // Render the texture to screen
  void updateScreen() const;

  [[nodiscard]] SDL_Window *getWindow() const { return window; }
  [[nodiscard]] SDL_Renderer *getRenderer() const { return renderer; }
  [[nodiscard]] SDL_Texture *getTexture() const { return texture; }

  [[nodiscard]] uint16_t getRenderWidth() const { return renderWidth; }
  [[nodiscard]] uint16_t getRenderHeight() const { return renderHeight; }

private:
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  SDL_Texture *texture = nullptr;

  uint16_t width = 0;// window width
  uint16_t height = 0;// window height
  uint16_t renderWidth = 0;
  uint16_t renderHeight = 0;

  static void ThrowSDL(const std::string &msg) { throw std::runtime_error(msg + ": " + SDL_GetError()); }
};