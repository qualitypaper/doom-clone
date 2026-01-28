#include <iostream>

#include "config.h"
#include "sdl_window.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

namespace sdl_window {
SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;

void updatePixels(uint32_t *pixels)
{
  SDL_UpdateTexture(texture, NULL, pixels, config::WINDOW_WIDTH * sizeof(uint32_t));
}

void updateScreen()
{
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
  // Not needed when using renderer - SDL_RenderPresent handles this
}

bool init()
{
  if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
    std::cout << "SDL failed to initialize, Error: " << SDL_GetError() << '\n';
    return false;
  }

  float_t mainScale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
  window = SDL_CreateWindow("Doom Clone",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    mainScale * config::WINDOW_WIDTH,
    mainScale * config::WINDOW_HEIGHT,
    SDL_WINDOW_SHOWN);

  if (!window) {
    std::cout << "SDL failed to create a window, Error: " << SDL_GetError() << '\n';
    return false;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) {
    // Fallback to software renderer
    std::cout << "SDL failed to create a renderer, Error: " << SDL_GetError() << '\n';
    return false;
  }

  texture = SDL_CreateTexture(
    renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, config::WINDOW_WIDTH, config::WINDOW_HEIGHT);

  if (!texture) {
    std::cout << "SDL failed to create a texture, Error: " << SDL_GetError() << '\n';
    return false;
  }

  IMGUI_CHECKVERSION();
  ImGuiContext *context = ImGui::CreateContext();
  ImGui::SetCurrentContext(context);

  ImGuiIO &io = ImGui::GetIO();
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


void createIMGUIFrame() {}

// must be called after init()
SDL_Window *getWindow() { return window; }

SDL_Renderer *getRenderer() { return renderer; }

void renderIMGUI(ImGuiIO &io)
{
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  ImGui::Render();
  SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
  SDL_SetRenderDrawColor(renderer,
    (Uint8)(clear_color.x * 255),
    (Uint8)(clear_color.y * 255),
    (Uint8)(clear_color.z * 255),
    (Uint8)(clear_color.w * 255));
  SDL_RenderClear(renderer);
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
  SDL_RenderPresent(renderer);
}
}// namespace sdl_window