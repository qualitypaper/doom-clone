#pragma once

#include "editor.h"
#include "editor_input_handler.h"
#include "gameloop.h"
#include "sdl_window.h"

#include <memory>
#include <vector>

struct ImVec2;

class EditorRenderer
{
public:
  EditorRenderer(SdlWindow &sdlWindow, Level &level);
  ~EditorRenderer();

  void render() const;

private:
  SdlWindow &m_sdlWindow;
  std::unique_ptr<Editor> m_editor;
  std::unique_ptr<EditorInputHandler> m_editorInputHandler;

  static void startFrame();
  void endFrame() const;

  void drawSidedefsWindow() const;
  void drawSectorsWindow() const;
  void drawSelectedLinePopup(uint32_t lineId) const;
  void drawSelectedVertexPopup(uint32_t selectedId) const;
  void drawMapOutlines(float_t vertexRadius, float_t thickness) const;
  void drawConnectedLineDefs(const std::vector<uint32_t> &connectedLineDefs,
    const EditorLineDef &ld,
    ImVec2 startDragged,
    ImVec2 endDragged,
    ImDrawList *drawList,
    float_t thickness) const;
  void drawSelectedVertex(uint32_t objectId, float_t vertexRadius, float_t thickness, ImDrawList *drawList) const;
  void drawSelectedLineDef(uint32_t objectId, float_t vertexRadius, float_t thickness, ImDrawList *drawList) const;
  void drawSelection(float_t vertexRadius, float_t thickness, ImDrawList *drawList) const;
  void drawUnselectedVertices(float_t vertexRadius, ImDrawList *drawList) const;
  void drawLinedef(const EditorLineDef &ld, ImDrawList *drawList, float_t thickness) const;
  void drawUnselectedLineDefs(float_t thickness, ImDrawList *drawList) const;

  void showVertexRLineCreation(const ImVec2 &mousePos, bool &isOpen) const;

  static void drawArrowForLinedef(ImDrawList *drawList,
    float_t thickness,
    const EditorVertex &startVertex,
    const EditorVertex &endVertex,
    ImU32 color);
  static void createSelect(const char *label,
    const std::vector<EditorSidedef> &sidedefs,
    int32_t &currentItem,
    bool hasReset = false);
  static constexpr ImVec2 scale(ImVec2 vec, float_t scaleFactor);
};
