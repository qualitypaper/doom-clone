#include "sdl_window.h"

#include "gameloop.h"
#include "imgui/backends/imgui_impl_sdl2.h"

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

  int w, h;
  SDL_GetRendererOutputSize(this->renderer, &w, &h);
  this->width = w;
  this->height = h;

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

void SdlWindow::UpdatePixels(const uint32_t *pixels) const
{
  // pitch = bytes per row
  SDL_UpdateTexture(texture, nullptr, pixels, static_cast<int>(renderWidth * sizeof(uint32_t)));
}

void SdlWindow::UpdateScreen() const
{
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}

void SdlWindow::PollEvents(InputState &input, bool &running)
{
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL2_ProcessEvent(&event);

    // process window quit/close early
    const bool isCloseWindow = event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE
      && event.window.windowID == SDL_GetWindowID(window);

    if (event.type == SDL_QUIT || isCloseWindow) {
      running = false;
      continue;
    }

    switch (event.type) {
    case SDL_WINDOWEVENT_RESIZED:
    case SDL_WINDOWEVENT_SIZE_CHANGED:
      width = event.window.data1;
      height = event.window.data2;
      break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      HandleKeyInput(event, input);
      break;
    case SDL_MOUSEMOTION:
      HandleMouseMovement(event, input);
      break;
    default:;
    }
  }
}
