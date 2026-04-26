#pragma once

#include "core/gameloop.h"
#include "core/math_utils.h"
#include "core/sdl_window.h"

#include <functional>
#include <memory>
#include <vector>


struct EditorSidedef;
struct EditorLineDef;
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
  void DrawMapOutlines(float_t vertexRadius, float_t thickness) const;

  void DrawCoordinatesCenter(float vertexRadius) const;
  void DrawLinePreview(float_t thickness) const;
  void drawVertices(float_t vertexRadius) const;
  void drawLinedef(const EditorLineDef &ld, float_t thickness) const;
  void drawLinedefs(float_t thickness) const;
  void DrawBlockSelection() const;
  void drawVertex(uint32_t vertexId, float vertexRadius) const;
  void drawArrowForLinedef(float_t thickness, uint32_t startVertexId, uint32_t endVertexId, ImU32 color) const;

  // popups
  void DrawSidedefsWindow() const;
  void DrawSectorsWindow() const;
  void DrawTexturesWindow() const;
  static void DrawPropertiesTable(const char *tableId, const char *columnLabel, const std::function<void()> &drawContent);
  static void SetUpFixedPosWindow(float xPos, float yPos, float width, float height, bool lockSize = true);
  [[nodiscard]] bool DrawSelectedLinePopup(uint32_t lineId) const;
  [[nodiscard]] bool DrawSelectedVertexPopup(uint32_t selectedId) const;
  void DrawPopupsForSelectedObjects() const;
  void DrawCreationPopup(const ImVec2 &mousePos, bool &isOpen) const;
  void DrawLevelSelection() const;

  // static methods
  static void CreateDropdown(const char *label,
                             std::string_view previewValue,
                             int itemCount,
                             const std::function<bool(int)> &isSelected,
                             const std::function<std::string(int)> &itemLabel,
                             const std::function<void(int)> &onSelect,
                             const std::function<void()> &drawFooter = nullptr);
  static void createSidedefSelect(const char *label,
    const std::vector<EditorSidedef> &sidedefs,
    int16_t &currentItem,
    bool hasReset = false);
};
