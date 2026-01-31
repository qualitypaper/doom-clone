#pragma once

#include "gameloop.h"
#include "sdl_window.h"

#include <vector>

struct ImVec4;
struct ImVec2;

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
  void createSelect(const char *label, const std::vector<gameloop::SideDef> &sidedefs, int16_t &currentItem);
  void drawSelectedLinePopup(int16_t selectedIndex, gameloop::Level &level);
  void drawMapOutlines(const std::vector<ImVec4> &scaledLinedefs, gameloop::Level &level, int16_t &selectedIndex, int16_t hoverIndex);

  constexpr ImVec2 scale(ImVec2 vec, float_t scaleFactor);
  float_t getDistanceToSegmentSq(ImVec2 start, ImVec2 end, ImVec2 origin);
};

}// namespace imguirenderer
