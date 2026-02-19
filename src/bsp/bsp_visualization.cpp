#include "bsp.h"
#include "config.h"
#include "imgui_impl_sdl2.h"
#include "imgui_renderer.h"
#include "math_utils.h"

#include <imgui.h>

void BSPBuilder::DrawBoundingBox(const SdlWindow &sdlWindow, const BspNode &root)
{
  const ImVec2 leftMin{ static_cast<float>(root.leftBoundingBox[3]), static_cast<float>(root.leftBoundingBox[2]) };
  const ImVec2 leftMax{ static_cast<float>(root.leftBoundingBox[1]), static_cast<float>(root.leftBoundingBox[0]) };

  const SDL_Rect left{ static_cast<int>(leftMin.x),
    static_cast<int>(leftMax.y),
    static_cast<int>(leftMax.x - leftMin.x),
    static_cast<int>(leftMin.y - leftMax.y) };

  const ImVec2 rightMin{ static_cast<float>(root.rightBoundingBox[3]), static_cast<float>(root.rightBoundingBox[2]) };
  const ImVec2 rightMax{ static_cast<float>(root.rightBoundingBox[1]), static_cast<float>(root.rightBoundingBox[0]) };

  const SDL_Rect right{ static_cast<int>(rightMin.x),
    static_cast<int>(rightMax.y),
    static_cast<int>(rightMax.x - rightMin.x),
    static_cast<int>(rightMin.y - rightMax.y) };

  SDL_SetRenderDrawColor(sdlWindow.getRenderer(), 0, 255, 0, 255);
  SDL_RenderDrawRect(sdlWindow.getRenderer(), &left);

  SDL_SetRenderDrawColor(sdlWindow.getRenderer(), 0, 255, 255, 255);
  SDL_RenderDrawRect(sdlWindow.getRenderer(), &right);
}

void BSPBuilder::DrawSubsectors(const SdlWindow &sdlWindow) const
{
  for (const auto &subsector : subsectors) {
    for (int i = subsector.firstSegIndex; i < subsector.segCount + subsector.firstSegIndex; i++) {
      const auto &seg = newSegments[i];
      const auto &startVertex = level->vertices[seg.startVertex];
      const auto &endVertex = level->vertices[seg.endVertex];

      const ImVec2 mappedStart = math_utils::fromCenterCoordinates(
        ImVec2{ static_cast<float>(startVertex.x), static_cast<float>(startVertex.y) },
        sdlWindow.width,
        sdlWindow.height);

      const ImVec2 mappedEnd = math_utils::fromCenterCoordinates(
        ImVec2{ static_cast<float>(endVertex.x), static_cast<float>(endVertex.y) }, sdlWindow.width, sdlWindow.height);

      SDL_RenderDrawLine(sdlWindow.getRenderer(), mappedStart.x, mappedStart.y, mappedEnd.x, mappedEnd.y);
    }
  }
}
void BSPBuilder::DrawSplittingLine(const SdlWindow &sdlWindow, const BspNode &root)
{
  constexpr float lineLen = 2000.0f;
  const float len = std::sqrt(static_cast<float>(root.dx) * root.dx + static_cast<float>(root.dy) * root.dy);
  const float dirX = static_cast<float>(root.dx) / len;
  const float dirY = static_cast<float>(root.dy) / len;

  const ImVec2 lineStart = math_utils::fromCenterCoordinates(
    ImVec2{ static_cast<float>(root.x) - dirX * lineLen, static_cast<float>(root.y) - dirY * lineLen },
    sdlWindow.width,
    sdlWindow.height);
  const ImVec2 lineEnd = math_utils::fromCenterCoordinates(
    ImVec2{ static_cast<float>(root.x) + dirX * lineLen, static_cast<float>(root.y) + dirY * lineLen },
    sdlWindow.width,
    sdlWindow.height);

  SDL_SetRenderDrawColor(sdlWindow.getRenderer(), 255, 255, 0, 255);
  SDL_RenderDrawLine(sdlWindow.getRenderer(),
    static_cast<int>(lineStart.x),
    static_cast<int>(lineStart.y),
    static_cast<int>(lineEnd.x),
    static_cast<int>(lineEnd.y));
}

void BSPBuilder::Visualize() const
{
  SdlWindow sdlWindow("BSP Builder", config::EDITOR_WINDOW_WIDTH, config::EDITOR_WINDOW_HEIGHT);
  const imguirenderer::ImguiRenderer imguiRenderer(sdlWindow, *level);

  const time_t start = time(nullptr);
  uint64_t prev = SDL_GetPerformanceCounter();
  const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
  constexpr double df = 1 / 144.0;
  bool running = true;

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) { ImGui_ImplSDL2_ProcessEvent(&event); }

    SDL_SetRenderDrawColor(sdlWindow.getRenderer(), 0, 255, 0, 255);

    DrawSubsectors(sdlWindow);

    const BspNode root = nodes[0];

    DrawSplittingLine(sdlWindow, root);
    DrawBoundingBox(sdlWindow, root);

    SDL_RenderPresent(sdlWindow.getRenderer());

    const uint64_t now = SDL_GetPerformanceCounter();
    double frameTime = static_cast<double>(now - prev) / freq;
    prev = now;
    if (frameTime > 0.25) frameTime = 0.25;

    if (frameTime < df) { SDL_Delay(static_cast<uint64_t>((df - frameTime) * 1000.0)); }
    running = time(nullptr) - start < 60 * 1000;
  }
}
