#pragma once

#include "editor.h"
#include "gameloop.h"
#include "sdl_window.h"

#include <memory>
#include <vector>

struct ImVec2;

namespace imguirenderer {

class ImguiRenderer
{
public:
  ImguiRenderer(sdl_window::SdlWindow &sdlWindow, gameloop::Level &level);
  ~ImguiRenderer();

  void render(const std::vector<ImVec2> &scaledLinedefs);

private:
  sdl_window::SdlWindow &m_sdlWindow;
  std::unique_ptr<editor::Editor> m_editor;
  gameloop::Level &m_level;

  void startFrame();
  void endFrame();
  void createSelect(const char *label, const std::vector<gameloop::SideDef> &sidedefs, int16_t &currentItem);
  void drawSelectedLinePopup(int16_t selectedIndex);
  void drawMapOutlines(const std::vector<ImVec2> &scaledLinedefs, int16_t &selectedIndex, int16_t hoverIndex);
  void showVertexRLineCreation(const ImVec2 &mousePos);

  constexpr ImVec2 scale(ImVec2 vec, float_t scaleFactor);
  float_t getDistanceToSegmentSq(ImVec2 start, ImVec2 end, ImVec2 origin);
};

}// namespace imguirenderer
