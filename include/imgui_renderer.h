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

  void render();

private:
  sdl_window::SdlWindow &m_sdlWindow;
  std::unique_ptr<editor::Editor> m_editor;

  void startFrame();
  void endFrame();
  void createSelect(const char *label,
    const std::vector<gameloop::SideDef> &sidedefs,
    uint32_t &currentItem,
    bool hasReset = false);
  void drawSelectedLinePopup(uint32_t selectedId);
  void drawSelectedVertexPopup(uint32_t selectedId);
  void drawMapOutlines();
  void showVertexRLineCreation(const ImVec2 &mousePos, bool &isOpen);

  constexpr ImVec2 scale(ImVec2 vec, float_t scaleFactor);
  float_t getDistanceToSegmentSq(ImVec2 start, ImVec2 end, ImVec2 origin);
};

}// namespace imguirenderer
