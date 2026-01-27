#include <iostream>

#include "config.h"
#include "sdl_window.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

namespace sdl_window {
SDL_Window *window;
SDL_Surface *surface;
SDL_Renderer *renderer;
SDL_Texture *texture;
ImGuiIO &io = ImGui::GetIO();

void updatePixels(uint32_t *pixels)
{
  SDL_UpdateTexture(texture, NULL, pixels, config::WINDOW_WIDTH * sizeof(uint32_t));
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

void updateScreen() { SDL_UpdateWindowSurface(window); }

bool init()
{
  if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
    std::cout << "SDL failed to initialize, Error: " << SDL_GetError() << '\n';
    return false;
  }

  float_t mainScale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
  window = SDL_CreateWindow("Doom Clone",
    SDL_WINDOWPOS_UNDEFINED,
    SDL_WINDOWPOS_UNDEFINED,
    mainScale * config::WINDOW_WIDTH,
    mainScale * config::WINDOW_HEIGHT,
    SDL_WINDOW_SHOWN);

  if (!window) {
    std::cout << "SDL failed to create a window, Error: " << SDL_GetError() << '\n';
    return false;
  }

  surface = SDL_GetWindowSurface(window);

  if (!surface) {
    std::cout << "SDL failed to create a window surface, Error: " << SDL_GetError() << '\n';
    return false;
  }

  renderer = SDL_CreateSoftwareRenderer(surface);
  if (!renderer) {
    std::cout << "SDL failed to create a software renderer, Error: " << SDL_GetError() << '\n';
    return false;
  }

  texture = SDL_CreateTexture(
    renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, config::WINDOW_WIDTH, config::WINDOW_HEIGHT);

  if (!texture) {
    std::cout << "SDL failed to create a texture, Error: " << SDL_GetError() << '\n';
    return false;
  }

  // print Renderer info
  // SDL_RendererInfo *info;
  // SDL_GetRendererInfo(renderer, info);
  // SDL_Log("Current SDL_Renderer: %s", info->name);

  // Setup Dear ImGUI context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;// Enable Keyboard Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Setup scaling
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(mainScale);// Bake a fixed style scale.
  style.FontScaleDpi = mainScale;// Set initial font scale.

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);

  return true;
}

void kill()
{
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyTexture(texture);

  SDL_Quit();
}


// must be called after init()
uint32_t getWindowId() { return SDL_GetWindowID(window); }

void renderIMGUI()
{
  ImGui::Render();
  SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
  // SDL_SetRenderDrawColor(renderer,
  //   (Uint8)(clear_color.x * 255),
  //   (Uint8)(clear_color.y * 255),
  //   (Uint8)(clear_color.z * 255),
  //   (Uint8)(clear_color.w * 255));
  SDL_RenderClear(renderer);
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
  SDL_RenderPresent(renderer);
}
}// namespace sdl_window