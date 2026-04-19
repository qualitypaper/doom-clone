#pragma once

#include "../../core/gameloop.h"
#include "../../core/math_utils.h"
#include "../../core/sdl_window.h"
#include "editor.h"

#include <functional>
#include <memory>
#include <vector>

struct ImVec2;


class EditorRenderer
{
public:
  EditorRenderer(std::shared_ptr<SdlWindow> sdlWindow, Editor &editor);
  ~EditorRenderer();

  void Render() const;

private:
  std::shared_ptr<SdlWindow> m_sdlWindow;
  Editor *m_editor;

  static void startFrame();
  void endFrame() const;

  // map drawing methods
  void drawMapOutlines(float_t vertexRadius, float_t thickness) const;

  void drawCoordinatesCenter(float vertexRadius) const;
  void drawLinePreview(float_t thickness) const;
  void drawVertices(float_t vertexRadius) const;
  void drawLinedef(const EditorLineDef &ld, float_t thickness) const;
  void drawLinedefs(float_t thickness) const;
  void drawBlockSelection() const;
  void drawVertex(uint32_t vertexId, float vertexRadius) const;
  void drawArrowForLinedef(float_t thickness, uint32_t startVertexId, uint32_t endVertexId, ImU32 color) const;

  // popups
  void drawSidedefsWindow() const;
  void drawSectorsWindow() const;
  void drawTexturesWindow() const;
  void drawPropertiesTable(const char *tableId, const char *columnLabel, const std::function<void()> &drawContent) const;
  [[nodiscard]] bool drawSelectedLinePopup(uint32_t lineId) const;
  [[nodiscard]] bool drawSelectedVertexPopup(uint32_t selectedId) const;
  void drawPopupsForSelectedObjects() const;
  void showVertexRLineCreation(const ImVec2 &mousePos, bool &isOpen) const;
  void drawLevelSelection() const;

  // static methods
  static void createSidedefSelect(const char *label,
    const std::vector<EditorSidedef> &sidedefs,
    int16_t &currentItem,
    bool hasReset = false);
  static void createSelect(const char *label,
    const std::vector<std::uint16_t> &options,
    std::uint16_t currentItem,
    const std::function<void(uint16_t)> &setCurrElem,
    const std::function<void()> &addNewElem);
};
