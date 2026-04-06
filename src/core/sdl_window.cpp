#include "sdl_window.h"

SdlWindow::SdlWindow(const char *title,
                     const uint16_t windowWidth,
                     const uint16_t windowHeight,
                     const uint16_t renderWidth,
                     const uint16_t renderHeight,
                     const uint32_t windowFlags)
  : renderWidth(renderWidth), renderHeight(renderHeight)
{
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    ThrowSDL("SDL_Init failed");
  }

  // Use provided size or desktop resolution
  if (windowWidth == 0 || windowHeight == 0) {
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
      ThrowSDL("SDL_GetDesktopDisplayMode failed");
    }
    width = dm.w;
    height = dm.h;
  } else {
    width = windowWidth;
    height = windowHeight;
  }

  this->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, windowFlags);
  if (!window) {
    SDL_Quit();
    ThrowSDL("SDL_CreateWindow failed");
  }

  this->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

  if (!renderer) {
    // Fallback to software only if hardware fails
    this->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
      SDL_DestroyWindow(window);
      SDL_Quit();
      ThrowSDL("Failed to create any renderer (accelerated or software)");
    }
  }

  // Create streaming texture at render resolution
  this->texture =
    SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, renderWidth, renderHeight);

  if (!texture) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    ThrowSDL("SDL_CreateTexture failed");
  }

  // SDL_RenderSetLogicalSize(renderer, renderWidth, renderHeight);
}

SdlWindow::~SdlWindow()
{
  if (texture)
    SDL_DestroyTexture(texture);
  if (renderer)
    SDL_DestroyRenderer(renderer);
  if (window)
    SDL_DestroyWindow(window);
  SDL_Quit();
}

void SdlWindow::updatePixels(const uint32_t *pixels) const
{
  // pitch = bytes per row
  SDL_UpdateTexture(texture, nullptr, pixels, static_cast<int>(renderWidth * sizeof(uint32_t)));
}

void SdlWindow::updateScreen() const
{
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}