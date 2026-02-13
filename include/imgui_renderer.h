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
  ImguiRenderer(SdlWindow &sdlWindow, Level &level);
  ~ImguiRenderer();

  void render() const;

private:
  SdlWindow &m_sdlWindow;
  std::unique_ptr<editor::Editor> m_editor;
  std::unique_ptr<editor::EditorInputHandler> m_editorInputHandler;

  static void startFrame();
  void endFrame() const;

  void drawSidedefsWindow() const;
  void drawSectorsWindow() const;
  void drawSelectedLinePopup(uint32_t selectedId) const;
  void drawSelectedVertexPopup(uint32_t selectedId) const;
  void drawMapOutlines(float_t vertexRadius, float_t thickness) const;
  void drawConnectedLineDefs(const std::vector<uint32_t> &connectedLineDefs,
    const editor::EditorLineDef &ld,
    ImVec2 startDragged,
    ImVec2 endDragged,
    ImDrawList *drawList,
    float_t thickness) const;
  void drawSelectedVertex(uint32_t objectId, float_t vertexRadius, float_t thickness, ImDrawList *drawList) const;
  void drawSelectedLineDef(uint32_t objectId, float_t vertexRadius, float_t thickness, ImDrawList *drawList) const;
  void drawSelection(float_t vertexRadius, float_t thickness, ImDrawList *drawList) const;
  void drawUnselectedVertices(float_t vertexRadius, ImDrawList *drawList) const;
  void drawUnselectedLineDefs(float_t thickness, ImDrawList *drawList) const;

  void showVertexRLineCreation(const ImVec2 &mousePos, bool &isOpen) const;

  static void createSelect(const char *label,
    const std::vector<SideDef> &sidedefs,
    int32_t &currentItem,
    bool hasReset = false);
  static constexpr ImVec2 scale(ImVec2 vec, float_t scaleFactor);
};

}// namespace imguirenderer
