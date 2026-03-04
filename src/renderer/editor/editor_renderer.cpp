#include "editor_renderer.h"

#include "editor.h"
#include "editor_input_handler.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "imgui_internal.h"
#include "math_utils.h"

#include <algorithm>
#include <fmt/core.h>
#include <memory>
#include <string>
#include <vector>

static ImU32 g_defaultColor = IM_COL32(255, 255, 255, 255);
static ImU32 g_selectedColor = IM_COL32(0, 0, 255, 255);
static ImU32 g_hoverColor = IM_COL32(255, 200, 0, 255);
static float g_defaultVertexRadius = 4.0f;
static float g_defaultLinedefThickness = 2.0f;

static std::vector<bool> s_renderedLinedefs;

EditorRenderer::EditorRenderer(SdlWindow &sdlWindow, Level &level) : m_sdlWindow(sdlWindow)
{
  const float_t mainScale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);

  IMGUI_CHECKVERSION();
  ImGuiContext *context = ImGui::CreateContext();
  ImGui::SetCurrentContext(context);

  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;// Enable Keyboard Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Setup scaling
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(mainScale);// Bake a fixed style scale.
  style.FontScaleDpi = mainScale;// Set initial font scale.

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForSDLRenderer(sdlWindow.getWindow(), sdlWindow.getRenderer());
  ImGui_ImplSDLRenderer2_Init(sdlWindow.getRenderer());

  this->m_editor = std::make_unique<Editor>(level, sdlWindow.width, sdlWindow.height);
  this->m_editorInputHandler = std::make_unique<EditorInputHandler>();
}

EditorRenderer::~EditorRenderer()
{
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
}

void EditorRenderer::startFrame()
{
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
}

void EditorRenderer::endFrame() const
{
  ImGui::Render();
  const ImGuiIO &currentIo = ImGui::GetIO();
  SDL_RenderSetScale(
    m_sdlWindow.getRenderer(), currentIo.DisplayFramebufferScale.x, currentIo.DisplayFramebufferScale.y);

  SDL_RenderClear(m_sdlWindow.getRenderer());
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_sdlWindow.getRenderer());
  SDL_RenderPresent(m_sdlWindow.getRenderer());
}

void EditorRenderer::drawLinePreview(const float_t thickness) const
{
  if (!m_editor->state->isCreatingLine) return;

  const auto &startVertex = m_editor->state->findVertex(m_editor->state->lineStartVertexId);

  const ImVec2 mousePos = ImGui::GetMousePos();

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddLine(startVertex.toImVec2(), mousePos, g_hoverColor, thickness);
}
void EditorRenderer::drawPopupsForSelectedObjects() const
{
  for (const auto &objectId : m_editor->state->selection) {
    const auto object = m_editor->state->findObject(objectId);
    if (!object) continue;

    if (object->type == EditorObjectType::VERTEX) {
      drawSelectedVertexPopup(objectId);
    } else if (object->type == EditorObjectType::LINEDEF) {
      drawSelectedLinePopup(objectId);
    }
  }
}
void EditorRenderer::render() const
{
  SDL_SetRenderDrawColor(m_sdlWindow.getRenderer(), 0, 0, 0, 255);

  // Start the Dear ImGui frame
  startFrame();
  s_renderedLinedefs.clear();
  s_renderedLinedefs.resize(m_editor->state->level->linedefs.size());

  const ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);

  const float_t thickness = g_defaultLinedefThickness * m_editor->state->canvasZoom;
  const float_t vertexRadius = g_defaultVertexRadius * m_editor->state->canvasZoom;

  // 2. Set flags
  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
                                     | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::Begin("HUDOverlay", nullptr, flags);

  // 3. Draw "screen-level" elements
  ImGui::TextColored(ImVec4(1, 1, 0, 1), "FPS: %.1f", io.Framerate);

  // for rendering the map of the level will be used a coordinate system which is rotated by 90 degrees
  // so (x, y) will be now (y, x)
  m_editor->processInput(vertexRadius);

  // render a window showing all the sectors
  drawSectorsWindow();

  // render a window showing all sidedefs
  drawSidedefsWindow();

  // suggest creating a new line/vertex
  if (m_editor->state->renderOptionsWindow) {
    showVertexRLineCreation(m_editor->state->optionsWindowPos, m_editor->state->renderOptionsWindow);
  }

  // draw map outlines
  drawMapOutlines(vertexRadius, thickness);

  // draw popups for selected objects
  drawPopupsForSelectedObjects();

  // draw line creation preview
  drawLinePreview(thickness);

  // draw block selection indication
  drawBlockSelection();

  ImGui::End();

  endFrame();
}

void EditorRenderer::drawBlockSelection() const
{
  if (!m_editor->state->isBlockSelecting) return;

  const auto drawList = ImGui::GetWindowDrawList();
  const ImVec2 blockSelectionStart = m_editor->state->blockSelectionStart;
  const ImVec2 blockSelectionEnd = { blockSelectionStart.x + m_editor->state->blockSelectionOffset.x,
    blockSelectionStart.y + m_editor->state->blockSelectionOffset.y };

  const float minX = std::min(blockSelectionStart.x, blockSelectionEnd.x);
  const float maxX = std::max(blockSelectionStart.x, blockSelectionEnd.x);
  const float minY = std::min(blockSelectionStart.y, blockSelectionEnd.y);
  const float maxY = std::max(blockSelectionStart.y, blockSelectionEnd.y);

  drawList->AddRectFilled({ minX, maxY }, { maxX, minY }, IM_COL32(100, 100, 0, 150));
}

void EditorRenderer::drawSidedefsWindow() const
{
  ImGui::Begin("SideDefs");

  if (ImGui::Button("Create sidedef")) {
    // create new sidedef
    m_editor->state->level->sidedefs.emplace_back(-1, 0, 0);
  }

  constexpr ImGuiTableFlags tableFlags =
    ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg
    | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;// Allows the list to scroll if it exceeds window height

  if (ImGui::BeginTable("PropertyTable", 1, tableFlags)) {
    ImGui::TableSetupColumn("Sidedefs table", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Setup columns: The right column stretches to fill available space
    // ---------------------------------------------------------
    // Property Category 1: Transform (Closed by default)
    // ---------------------------------------------------------
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);

    // Flags for the parent category
    // - SpanFullWidth: Allows clicking anywhere on the row to open/close
    // - We omit ImGuiTreeNodeFlags_DefaultOpen so it stays closed initially
    constexpr ImGuiTreeNodeFlags categoryFlags = ImGuiTreeNodeFlags_SpanFullWidth;

    for (size_t i = 0; i < m_editor->state->level->sidedefs.size(); i++) {
      auto &sd = m_editor->state->level->sidedefs[i];
      std::string label = "Sidedef " + std::to_string(i) + "##Sidedef";
      ImGui::PushID(i);
      const bool isTransformOpen = ImGui::TreeNodeEx(label.c_str(), categoryFlags);

      if (isTransformOpen) {
        // Sub-property: Position
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        int sectorId = sd.sectorId;
        int xOffset = sd.xOffset;
        int yOffset = sd.yOffset;

        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Sector id", &sectorId)) {
          // update sector id
          sd.sectorId = static_cast<int16_t>(sectorId);
        }

        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Texture offset x", &xOffset)) {
          // update texture offset x
          sd.xOffset = static_cast<int16_t>(xOffset);
        }
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Texture offset y", &yOffset)) {
          // update texture offset y
          sd.yOffset = static_cast<int16_t>(yOffset);
        }

        if (ImGui::Button("Delete")) {
          for (auto &linedef : m_editor->state->level->linedefs) {
            if (linedef.backSideDef == static_cast<int32_t>(i)) {
              linedef.backSideDef = -1;
            } else if (linedef.frontSideDef == static_cast<int32_t>(i)) {
              linedef.frontSideDef = -1;
            }
          }

          m_editor->state->level->sidedefs.erase(m_editor->state->level->sidedefs.begin() + i);
        }

        ImGui::NewLine();

        // Pop the parent node
        ImGui::TreePop();
      }

      ImGui::PopID();
    }

    ImGui::EndTable();
  }

  ImGui::End();
}

void EditorRenderer::drawSectorsWindow() const
{

  ImGui::Begin("Sectors");

  if (ImGui::Button("Create sector")) { m_editor->state->level->sectors.emplace_back(); }
  //
  // for (uint32_t sectorId = 0; sectorId < m_editor->state->level->sectors.size(); sectorId++) {
  //   auto &sector = m_editor->state->findSector(sectorId);
  //   ImGui::PushID(sectorId);
  //
  //   ImGui::Text("Sector: %d", sectorId);
  //   ImGui::SetNextItemWidth(100);
  //   int floorHeight = sector.floorHeight;
  //   int ceilingHeight = sector.ceilingHeight;
  //   if (ImGui::InputInt("Sector floor height", &floorHeight)) {
  //     sector.floorHeight = static_cast<int16_t>(floorHeight);
  //   }
  //   ImGui::SetNextItemWidth(100);
  //   if (ImGui::InputInt("Sector ceiling height", &ceilingHeight)) {
  //     sector.ceilingHeight = static_cast<int16_t>(ceilingHeight);
  //   }
  //
  //   float color[3] = { ImGui::ColorConvertU32ToFloat4(sector.color).x,
  //     ImGui::ColorConvertU32ToFloat4(sector.color).y,
  //     ImGui::ColorConvertU32ToFloat4(sector.color).z };
  //
  //   if (ImGui::ColorPicker3("Sector color", color)) {
  //     // update sidedef color
  //     sector.color = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], 1.0f));
  //   }
  //
  //   ImGui::NewLine();
  //   ImGui::PopID();
  // }

  constexpr ImGuiTableFlags tableFlags =
    ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg
    | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;// Allows the list to scroll if it exceeds window height

  if (ImGui::BeginTable("PropertyTable", 1, tableFlags)) {
    ImGui::TableSetupColumn("Sectors table", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Setup columns: The right column stretches to fill available space
    // ---------------------------------------------------------
    // Property Category 1: Transform (Closed by default)
    // ---------------------------------------------------------
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);

    // Flags for the parent category
    // - SpanFullWidth: Allows clicking anywhere on the row to open/close
    // - We omit ImGuiTreeNodeFlags_DefaultOpen so it stays closed initially
    constexpr ImGuiTreeNodeFlags categoryFlags = ImGuiTreeNodeFlags_SpanFullWidth;

    for (size_t i = 0; i < m_editor->state->level->sectors.size(); i++) {
      EditorSector &sector = m_editor->state->level->sectors[i];
      std::string label = "Sector " + std::to_string(i) + "##Sector";
      ImGui::PushID(i);
      const bool isTransformOpen = ImGui::TreeNodeEx(label.c_str(), categoryFlags);

      if (isTransformOpen) {
        // Sub-property: Position
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);


        int floorHeight = sector.floorHeight;
        int ceilingHeight = sector.ceilingHeight;
        int lightLevel = sector.lightLevel;

        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Floor Height", &floorHeight)) {
          // update sector id
          sector.floorHeight = static_cast<int16_t>(floorHeight);
        }

        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Ceiling height", &ceilingHeight)) {
          // update texture offset x
          sector.ceilingHeight = static_cast<int16_t>(ceilingHeight);
        }

        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Light level", &lightLevel)) {
          // update texture offset y
          sector.lightLevel = static_cast<int16_t>(lightLevel);
        }

        ImGui::SetNextItemWidth(200);

        float color[3] = { ImGui::ColorConvertU32ToFloat4(sector.color).x,
          ImGui::ColorConvertU32ToFloat4(sector.color).y,
          ImGui::ColorConvertU32ToFloat4(sector.color).z };

        if (ImGui::ColorPicker3("Sector color", color)) {
          // update sector color
          sector.color = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], 1.0f));
        }

        if (ImGui::Button("Delete")) {
          for (auto &linedef : m_editor->state->level->linedefs) {
            if (linedef.backSideDef == static_cast<int32_t>(i)) {
              linedef.backSideDef = -1;
            } else if (linedef.frontSideDef == static_cast<int32_t>(i)) {
              linedef.frontSideDef = -1;
            }
          }

          m_editor->state->level->sidedefs.erase(m_editor->state->level->sidedefs.begin() + i);
        }

        ImGui::NewLine();

        // Pop the parent node
        ImGui::TreePop();
      }

      ImGui::PopID();
    }

    ImGui::EndTable();
  }

  ImGui::End();
}

// draws the neighboring linedefs that must point now into new dragged point
void EditorRenderer::drawConnectedLineDefs(const std::vector<uint32_t> &connectedLineDefs,
  const EditorLineDef &ld,
  const ImVec2 startDragged,
  const ImVec2 endDragged,
  const float_t thickness) const
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();

  for (auto &otherLdObjectId : connectedLineDefs) {
    const auto otherLdIndex = getObjectIndex(otherLdObjectId);
    if (s_renderedLinedefs[otherLdIndex]) continue;

    const auto &otherLd = m_editor->state->findLinedef(otherLdObjectId);
    if (otherLd.selected || (otherLd.start == ld.start && otherLd.end == ld.end)) continue;

    auto otherStart = m_editor->state->findVertex(otherLd.start);
    auto otherEnd = m_editor->state->findVertex(otherLd.end);

    auto otherStartDragged = otherStart + m_editor->state->draggingOffset;
    auto otherEndDragged = otherEnd + m_editor->state->draggingOffset;

    s_renderedLinedefs[otherLdIndex] = true;

    const bool bothSelected = (otherStart.selected || otherStart.isAnyConnectedLineDefSelected(*m_editor->state))
                              && (otherEnd.selected || otherEnd.isAnyConnectedLineDefSelected(*m_editor->state));

    if (otherLd.start == ld.start) {
      if (bothSelected) {
        drawList->AddLine(startDragged, otherEndDragged.toImVec2(), g_defaultColor, thickness);
      } else {
        drawList->AddLine(startDragged, otherEnd.toImVec2(), g_defaultColor, thickness);
      }
    } else if (otherLd.end == ld.start) {
      if (bothSelected) {
        drawList->AddLine(startDragged, otherStartDragged.toImVec2(), g_defaultColor, thickness);
      } else {
        drawList->AddLine(startDragged, otherStart.toImVec2(), g_defaultColor, thickness);
      }
    } else if (otherLd.start == ld.end) {
      if (bothSelected) {
        drawList->AddLine(endDragged, otherEndDragged.toImVec2(), g_defaultColor, thickness);
      } else {
        drawList->AddLine(endDragged, otherEnd.toImVec2(), g_defaultColor, thickness);
      }
    } else if (otherLd.end == ld.end) {
      if (bothSelected) {
        drawList->AddLine(endDragged, otherStartDragged.toImVec2(), g_defaultColor, thickness);
      } else {
        drawList->AddLine(endDragged, otherStart.toImVec2(), g_defaultColor, thickness);
      }
    }
  }
}

void EditorRenderer::drawSelectedVertex(const uint32_t objectId,
  const float_t vertexRadius,
  const float_t thickness) const
{

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  const auto &v = m_editor->state->findVertex(objectId);

  EditorVertex dragged(static_cast<float_t>(v.x) + m_editor->state->draggingOffset.x,
    static_cast<float_t>(v.y) + m_editor->state->draggingOffset.y);
  dragged.hovered = v.hovered;
  dragged.selected = v.selected;

  drawVertex(dragged, vertexRadius);

  // draw every linedef connected to this vertex
  for (const uint32_t ldId : v.connectedLineDefs) {
    auto &ld = m_editor->state->findLinedef(ldId);
    if (ld.selected) continue;

    const auto otherVertexId = ld.start == objectId ? ld.end : ld.start;
    auto &otherVertex = m_editor->state->findVertex(otherVertexId);

    drawList->AddLine(dragged.toImVec2(), otherVertex.toImVec2(), g_selectedColor, thickness);
  }
}
void EditorRenderer::drawSelectedLineDef(const uint32_t objectId,
  const float_t vertexRadius,
  const float_t thickness) const
{
  const auto index = getObjectIndex(objectId);
  if (s_renderedLinedefs[index]) return;

  ImDrawList *drawList = ImGui::GetWindowDrawList();

  const auto &ld = m_editor->state->findLinedef(objectId);
  const auto start = m_editor->state->findVertex(ld.start);
  const auto end = m_editor->state->findVertex(ld.end);

  const EditorVertex startDragged = start + m_editor->state->draggingOffset;
  const EditorVertex endDragged = end + m_editor->state->draggingOffset;

  const ImVec2 startVec = startDragged.toImVec2();
  const ImVec2 endVec = endDragged.toImVec2();

  drawList->AddLine(startVec, endVec, g_selectedColor, thickness);

  drawList->AddCircleFilled(startVec, vertexRadius, g_defaultColor);
  drawList->AddCircleFilled(endVec, vertexRadius, g_defaultColor);

  s_renderedLinedefs[index] = true;

  // find all the linedefs connected to end/start and draw them to this line
  drawConnectedLineDefs(start.connectedLineDefs, ld, startVec, endVec, thickness);
  drawConnectedLineDefs(end.connectedLineDefs, ld, startVec, endVec, thickness);
}
void EditorRenderer::drawSelection(const float_t vertexRadius, const float_t thickness) const
{
  for (const uint32_t objectId : m_editor->state->selection) {
    const EditorObjectType type = getObjectType(objectId);

    switch (type) {
    case EditorObjectType::LINEDEF: {
      drawSelectedLineDef(objectId, vertexRadius, thickness);
      break;
    }
    case EditorObjectType::VERTEX: {
      drawSelectedVertex(objectId, vertexRadius, thickness);
      break;
    }
    default:
      throw std::runtime_error("Selected an unexpected/unknown type.");
    }
  }
}

void EditorRenderer::drawUnselectedVertices(const float_t vertexRadius) const
{
  for (const EditorVertex &v : m_editor->state->level->vertices) {
    // selected vertices are processed separately
    if (v.selected || v.isAnyConnectedLineDefSelected(*m_editor->state)) { continue; }

    drawVertex(v, vertexRadius);
  }
}

void EditorRenderer::drawArrowForLinedef(const float_t thickness,
  const EditorVertex &startVertex,
  const EditorVertex &endVertex,
  const ImU32 color)
{
  static double s_arrowLength = 15;
  static double s_arrowAngle = 135;

  EditorVertex linedefDir{ endVertex.x - startVertex.x, endVertex.y - startVertex.y };
  linedefDir.normalize();

  const ImVec2 leftArrowDir = math_utils::rotateAroundX(linedefDir, s_arrowAngle);
  const ImVec2 rightArrowDir = math_utils::rotateAroundX(linedefDir, -s_arrowAngle);

  const ImVec2 leftArrowEnd{ static_cast<float>(endVertex.x + leftArrowDir.x * s_arrowLength),
    static_cast<float>(endVertex.y + leftArrowDir.y * s_arrowLength) };
  const ImVec2 rightArrowEnd{ static_cast<float>(endVertex.x + rightArrowDir.x * s_arrowLength),
    static_cast<float>(endVertex.y + rightArrowDir.y * s_arrowLength) };

  ImDrawList *drawList = ImGui::GetWindowDrawList();

  drawList->AddLine(endVertex.toImVec2(), leftArrowEnd, color, thickness / 2.0f);
  drawList->AddLine(endVertex.toImVec2(), rightArrowEnd, color, thickness / 2.0f);
}

void EditorRenderer::drawLinedef(const EditorLineDef &ld, const float_t thickness) const
{
  const EditorVertex startVertex = m_editor->state->findVertex(ld.start);
  const EditorVertex endVertex = m_editor->state->findVertex(ld.end);

  ImU32 color;
  if (ld.selected) {
    color = g_selectedColor;
  } else if (ld.hovered) {
    color = g_hoverColor;
  } else {
    color = g_defaultColor;
  }

  // tint a bit the color of the portal linedef
  if (ld.backSideDef != -1) { color -= 0x32323200; }

  drawLineWithZoom(startVertex.toImVec2(), endVertex.toImVec2(), color, thickness);

  // draw a small arrow showing the direction of the linedef
  drawArrowForLinedef(thickness, startVertex, endVertex, color);
}

void EditorRenderer::drawLineWithZoom(const ImVec2 &start,
  const ImVec2 &end,
  const ImU32 color,
  const float thickness) const
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();

  const float zoom = m_editor->state->canvasZoom;
  if (zoom == 1.0f) {
    drawList->AddLine(start, end, color, thickness);
    return;
  }

  const uint16_t width = m_sdlWindow.width, height = m_sdlWindow.height;

  ImVec2 startZoomed = math_utils::toCenterCoordinates(start, width, height);
  ImVec2 endZoomed = math_utils::toCenterCoordinates(end, width, height);

  startZoomed = { startZoomed.x * zoom, startZoomed.y * zoom };
  endZoomed = { endZoomed.x * zoom, endZoomed.y * zoom };

  drawList->AddLine(math_utils::fromCenterCoordinates(startZoomed, width, height),
    math_utils::fromCenterCoordinates(endZoomed, width, height),
    color,
    thickness);
}

void EditorRenderer::drawUnselectedLineDefs(const float_t thickness) const
{
  for (size_t i = 0; i < m_editor->state->level->linedefs.size(); i++) {
    if (i < s_renderedLinedefs.size() && s_renderedLinedefs[i]) continue;

    const auto &ld = m_editor->state->level->linedefs[i];
    const auto startVertex = m_editor->state->findVertex(ld.start);
    const auto endVertex = m_editor->state->findVertex(ld.end);

    // selected linedefs are processed separately
    if (startVertex.selected || endVertex.selected || ld.selected
        || startVertex.isAnyConnectedLineDefSelected(*m_editor->state)
        || endVertex.isAnyConnectedLineDefSelected(*m_editor->state)) {
      continue;
    }

    if (i < s_renderedLinedefs.size()) { s_renderedLinedefs[i] = true; }
    drawLinedef(ld, thickness);
  }
}
void EditorRenderer::drawMapOutlines(const float_t vertexRadius, const float_t thickness) const
{
  // draw selected vertices/linedefs
  drawSelection(vertexRadius, thickness);
  // draw outlines
  drawUnselectedLineDefs(thickness);
  // draw vertices
  drawUnselectedVertices(vertexRadius);
}

void EditorRenderer::showVertexRLineCreation(const ImVec2 &mousePos, bool &isOpen) const
{
  ImGui::SetNextWindowPos(mousePos);
  ImGui::Begin("Vertex/Line creation popup");
  const bool vertexCreation = ImGui::Button("Create Vertex");
  const bool lineCreation = ImGui::Button("Create Line");

  if (vertexCreation) {
    m_editor->addVertex(static_cast<int16_t>(mousePos.x), static_cast<int16_t>(mousePos.y));
    isOpen = false;
  } else if (lineCreation) {
    // TODO:
  }

  ImGui::End();
}

void EditorRenderer::drawSelectedLinePopup(const uint32_t lineId) const
{
  const auto index = getObjectIndex(lineId);
  auto &ld = m_editor->state->findLinedef(index);

  auto &start = m_editor->state->findVertex(ld.start);
  auto &end = m_editor->state->findVertex(ld.end);

  // render popup of linedef parameters
  const std::string windowTitle = "Linedef params " + std::to_string(index) + "###LinedefParams";
  ImGui::Begin(windowTitle.c_str());
  ImGui::PushID(std::to_string(lineId).c_str());

  ImGui::Text("LineDef params: %d", index);

  ImGui::SetNextItemWidth(80);
  ImGui::InputInt(": Start X", &start.x);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  ImGui::InputInt(": Start Y", &start.y);

  ImGui::NewLine();

  ImGui::SetNextItemWidth(80);
  ImGui::InputInt("End X: ", &end.x);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  ImGui::InputInt("End Y: ", &end.y);

  if (ImGui::Button("Swap")) {
    const uint32_t temp = ld.start;
    ld.start = ld.end;
    ld.end = temp;
  }

  ImGui::NewLine();

  const std::string frontSideDefLabel = fmt::format(":Front SideDef ##LDSidedef{0}", ld.start);
  const std::string backSideDefLabel = fmt::format(":Back SideDef ##LDSidedef{0}", ld.end);

  createSelect(frontSideDefLabel.c_str(), m_editor->state->level->sidedefs, ld.frontSideDef);
  createSelect(backSideDefLabel.c_str(), m_editor->state->level->sidedefs, ld.backSideDef, true);

  if (ImGui::Button("Delete")) { EditorLineDef::remove(*m_editor->state, index); }

  ImGui::NewLine();
  ImGui::NewLine();

  ImGui::PopID();
  ImGui::End();
}

void EditorRenderer::drawSelectedVertexPopup(const uint32_t selectedId) const
{
  const uint32_t index = getObjectIndex(selectedId);

  const auto &vertex = m_editor->state->findVertex(index);
  const std::string windowTitle = "Vertex params: " + std::to_string(index) + "###Vertex" + std::to_string(index);
  ImGui::Begin(windowTitle.c_str());

  ImGui::PushID(std::to_string(selectedId).c_str());

  for (const auto &ldObjectId : vertex.connectedLineDefs) {
    const auto ldIndex = getObjectIndex(ldObjectId);

    ImGui::Text("Connected LineDef ID: %u", ldIndex);
  }

  if (ImGui::Button("Create Connected Line")) { m_editor->drawConnectedLine(index); }
  if (ImGui::Button("Delete")) { EditorVertex::remove(*m_editor->state, index); }

  ImGui::PopID();
  ImGui::End();
}

void EditorRenderer::createSelect(const char *label,
  const std::vector<EditorSidedef> &sidedefs,
  int32_t &currentItem,
  const bool hasReset)
{
  if (ImGui::BeginCombo(label, std::to_string(currentItem).c_str())) {
    // handling -1 option separately
    if (hasReset || currentItem == -1) {
      const bool isReset = currentItem == -1;

      const auto resetLabel = "-1";
      if (ImGui::Selectable(resetLabel, isReset)) { currentItem = -1; }
    }

    for (size_t i = 0; i < sidedefs.size(); i++) {
      const bool is_selected = static_cast<size_t>(currentItem) == i;
      std::string optionLabel = std::to_string(i);

      if (ImGui::Selectable(optionLabel.c_str(), is_selected)) { currentItem = static_cast<int32_t>(i); }

      // Set the initial focus when opening the combo (scrolling to selection)
      if (is_selected) { ImGui::SetItemDefaultFocus(); }
    }
    ImGui::EndCombo();
  }
}

constexpr ImVec2 scale(const ImVec2 vec, const float_t scaleFactor)
{ return { scaleFactor * vec.x, scaleFactor * vec.y }; }


void EditorRenderer::drawVertex(const EditorVertex &vertex, const float vertexRadius = g_defaultVertexRadius) const
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();

  ImU32 color;

  if (vertex.selected) {
    color = g_selectedColor;
  } else if (vertex.hovered) {
    color = g_hoverColor;
  } else {
    color = g_defaultColor;
  }

  const float zoom = m_editor->state->canvasZoom;
  ImVec2 centered = math_utils::toCenterCoordinates(vertex, m_sdlWindow.width, m_sdlWindow.height);

  centered = { centered.x * zoom, centered.y * zoom };

  drawList->AddCircleFilled(
    math_utils::fromCenterCoordinates(centered, m_sdlWindow.width, m_sdlWindow.height), vertexRadius, color);
}
