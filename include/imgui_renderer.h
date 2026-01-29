#pragma once

#include "sdl_window.h"
#include "gameloop.h"

#include <vector>

typedef struct ImVec4;
typedef struct ImVec2;

namespace imguirenderer {

class ImguiRenderer
{
public:
  ImguiRenderer(sdl_window::SdlWindow &sdlWindow);
  ~ImguiRenderer();

  void render(gameloop::Level &level, const std::vector<ImVec4> &scaledLinedefs);

private:
  sdl_window::SdlWindow &sdlWindow;

  void startFrame();
  void endFrame();
  void drawHUD();

  constexpr ImVec2 scale(ImVec2 vec, float_t scaleFactor);
  float_t getDistanceToSegmentSq(ImVec2 start, ImVec2 end, ImVec2 origin);
};

}// namespace imguirenderer