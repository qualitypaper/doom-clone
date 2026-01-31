#include <iostream>

#include "config.h"
#include "sdl_window.h"

namespace sdl_window {


void SdlWindow::updatePixels(uint32_t *pixels) { SDL_UpdateTexture(texture, NULL, pixels, width * sizeof(uint32_t)); }

void SdlWindow::updateScreen()
{
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
  // Not needed when using renderer - SDL_RenderPresent handles this
}

SdlWindow::SdlWindow(int16_t width, int16_t height) : width(width), height(height)
{
  if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
    std::runtime_error("SDL failed to initialize, Error: " + std::string(SDL_GetError()));
  }

  window = SDL_CreateWindow("Doom Clone",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    width,
    height,
    SDL_WINDOW_SHOWN);

  if (!window) { std::runtime_error("SDL failed to create a window, Error: " + std::string(SDL_GetError())); }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) {
    // Fallback to software renderer
    std::runtime_error("SDL failed to create a renderer, Error: " + std::string(SDL_GetError()));
  }

  texture = SDL_CreateTexture(
    renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, config::WINDOW_WIDTH, config::WINDOW_HEIGHT);

  if (!texture) { std::runtime_error("SDL failed to create a texture, Error: " + std::string(SDL_GetError())); }
}

SdlWindow::~SdlWindow()
{
  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyTexture(texture);

  SDL_Quit();
}

// must be called after init()
SDL_Window *SdlWindow::getWindow() { return window; }

SDL_Renderer *SdlWindow::getRenderer() { return renderer; }

}// namespace sdl_window
