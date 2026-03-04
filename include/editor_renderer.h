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

  // map drawing methods
  void drawMapOutlines(float_t vertexRadius, float_t thickness) const;

  void drawLinePreview(float_t thickness) const;
  void drawConnectedLineDefs(const std::vector<uint32_t> &connectedLineDefs,
    const EditorLineDef &ld,
    ImVec2 startDragged,
    ImVec2 endDragged,
    float_t thickness) const;
  void drawSelectedVertex(uint32_t objectId, float_t vertexRadius, float_t thickness) const;
  void drawSelectedLineDef(uint32_t objectId, float_t vertexRadius, float_t thickness) const;
  void drawSelection(float_t vertexRadius, float_t thickness) const;
  void drawUnselectedVertices(float_t vertexRadius) const;
  void drawLinedef(const EditorLineDef &ld, float_t thickness) const;
  void drawLineWithZoom(const ImVec2 &start, const ImVec2 &end, ImU32 color, float thickness) const;
  void drawUnselectedLineDefs(float_t thickness) const;
  void drawBlockSelection() const;

  // popups
  void drawSidedefsWindow() const;
  void drawSectorsWindow() const;
  void drawSelectedLinePopup(uint32_t lineId) const;
  void drawSelectedVertexPopup(uint32_t selectedId) const;
  void drawPopupsForSelectedObjects() const;
  void showVertexRLineCreation(const ImVec2 &mousePos, bool &isOpen) const;

  // static methods
  void drawVertex(const EditorVertex &vertex, float vertexRadius) const;
  static void
    drawArrowForLinedef(float_t thickness, const EditorVertex &startVertex, const EditorVertex &endVertex, ImU32 color);

  static void createSelect(const char *label,
    const std::vector<EditorSidedef> &sidedefs,
    int32_t &currentItem,
    bool hasReset = false);
};
