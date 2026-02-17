#include <iostream>

#include "config.h"
#include "sdl_window.h"

void SdlWindow::updatePixels(const uint32_t *pixels) const
{ SDL_UpdateTexture(texture, nullptr, pixels, width * sizeof(uint32_t)); }

void SdlWindow::updateScreen() const
{
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
  // Not needed when using renderer - SDL_RenderPresent handles this
}

SdlWindow::SdlWindow(const uint16_t _width, const uint16_t _height, const uint32_t flags)
{
  if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
    throw std::runtime_error("SDL failed to initialize, Error: " + std::string(SDL_GetError()));
  }

  if (_width == 0 || _height == 0) {
    SDL_DisplayMode dm;
    SDL_GetDesktopDisplayMode(0, &dm);

    width = dm.w;
    height = dm.h;
  } else {
    width = _width;
    height = _height;
  }

  window = SDL_CreateWindow("Doom Clone", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);

  if (!window) { throw std::runtime_error("SDL failed to create a window, Error: " + std::string(SDL_GetError())); }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) {
    // Fallback to software renderer
    throw std::runtime_error("SDL failed to create a renderer, Error: " + std::string(SDL_GetError()));
  }

  texture = SDL_CreateTexture(
    renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, config::WINDOW_WIDTH, config::WINDOW_HEIGHT);

  if (!texture) { throw std::runtime_error("SDL failed to create a texture, Error: " + std::string(SDL_GetError())); }
}

SdlWindow::~SdlWindow()
{
  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyTexture(texture);

  SDL_Quit();
}

// must be called after init()
SDL_Window *SdlWindow::getWindow() const { return window; }

SDL_Renderer *SdlWindow::getRenderer() const { return renderer; }
SDL_Texture *SdlWindow::getTexture() const { return texture; }
