#pragma once

#include "editor.h"
#include "editor_input_handler.h"
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

  void render() const;

private:
  sdl_window::SdlWindow &m_sdlWindow;
  std::unique_ptr<editor::Editor> m_editor;
  std::unique_ptr<editor::EditorInputHandler> m_editorInputHandler;

  static void startFrame();
  void endFrame() const;
  static void createSelect(const char *label,
    const std::vector<gameloop::SideDef> &sidedefs,
    int32_t &currentItem,
    bool hasReset = false);
  void drawSelectedLinePopup(uint32_t selectedId) const;
  void drawSelectedVertexPopup(uint32_t selectedId) const;
  void drawMapOutlines() const;
  void drawSelection(ImDrawList *drawList, float_t s_vertexRadius, float_t s_thickness);
  void drawVertex(ImDrawList *drawList, ImVec2 &vertex, float_t vertexRadius, ImU32 color);
  void drawLine(ImDrawList *drawList, ImVec2 &start, editor::EditorVertex &end, ImU32 color, float_t thickness);
  void showVertexRLineCreation(const ImVec2 &mousePos, bool &isOpen) const;
  void drawConnectedLineDefs(const editor::EditorLineDef &ld,
    const std::vector<uint32_t> &connectedLineDefs,
    ImVec2 startDragged,
    ImVec2 endDragged,
    ImDrawList *drawList,
    float_t thickness) const;

  constexpr ImVec2 scale(ImVec2 vec, float_t scaleFactor);
  float_t getDistanceToSegmentSq(ImVec2 start, ImVec2 end, ImVec2 origin);
};

}// namespace imguirenderer
