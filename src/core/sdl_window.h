#pragma once

#include <SDL.h>
#include <array>
#include <memory>
#include <stdexcept>

struct InputState
{
  std::array<bool, SDL_NUM_SCANCODES> keys;
  std::array<bool, 8> mouseButtons;
  int mousedx;
  int mousedy;
};

class SdlWindow
{
public:
  SdlWindow(const char *title,
            uint16_t windowWidth,// size of the actual window
            uint16_t windowHeight,
            uint16_t renderWidth,// internal render resolution
            uint16_t renderHeight,
            uint32_t windowFlags = 0);// e.g. SDL_WINDOW_RESIZABLE

  ~SdlWindow();

  void UpdatePixels(const uint32_t *pixels) const;

  // Render the texture to screen
  void UpdateScreen() const;
  void PollEvents(InputState &input, bool &running);

  [[nodiscard]] SDL_Window *GetWindow() const { return window; }
  [[nodiscard]] SDL_Renderer *GetRenderer() const { return renderer; }
  [[nodiscard]] SDL_Texture *GetTexture() const { return texture; }

  [[nodiscard]] uint16_t GetRenderWidth() const { return renderWidth; }
  [[nodiscard]] uint16_t GetRenderHeight() const { return renderHeight; }

  [[nodiscard]] uint16_t GetWidth() const { return width; }
  [[nodiscard]] uint16_t GetHeight() const { return height; }

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