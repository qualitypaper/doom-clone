#include "imgui_renderer.h"

#include "config.h"
#include "editor.h"
#include "editor_input_handler.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "imgui_internal.h"

#include <algorithm>
#include <array>
#include <fmt/core.h>
#include <memory>
#include <string>
#include <vector>

namespace imguirenderer {
static ImU32 g_defaultColor = IM_COL32(255, 255, 255, 255);
static ImU32 g_selectedColor = IM_COL32(0, 0, 255, 255);
static ImU32 g_hoverColor = IM_COL32(255, 200, 0, 255);

ImguiRenderer::ImguiRenderer(sdl_window::SdlWindow &sdlWindow, gameloop::Level &level) : m_sdlWindow(sdlWindow)
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

  this->m_editor = std::make_unique<editor::Editor>(level, sdlWindow.width, sdlWindow.height);
  this->m_editorInputHandler = std::make_unique<editor::EditorInputHandler>();
}

ImguiRenderer::~ImguiRenderer()
{
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
}

void ImguiRenderer::startFrame()
{
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
}

void ImguiRenderer::endFrame() const
{
  ImGui::Render();
  const ImGuiIO &currentIo = ImGui::GetIO();
  SDL_RenderSetScale(
    m_sdlWindow.getRenderer(), currentIo.DisplayFramebufferScale.x, currentIo.DisplayFramebufferScale.y);

  SDL_RenderClear(m_sdlWindow.getRenderer());
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_sdlWindow.getRenderer());
  SDL_RenderPresent(m_sdlWindow.getRenderer());
}

void ImguiRenderer::render() const
{
  // Start the Dear ImGui frame
  startFrame();

  const ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);

  const float_t thickness = 2.0f * m_editor->state->canvasZoom;
  const float_t vertexRadius = 4.0f * m_editor->state->canvasZoom;

  // 2. Set flags to make it invisible and non-interactive
  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
                                     | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::Begin("HUDOverlay", nullptr, flags);

  // 3. Draw your "screen-level" elements
  ImGui::TextColored(ImVec4(1, 1, 0, 1), "FPS: %.1f", io.Framerate);

  // for rendering the map of the level will be used a coordinate system which is rotated by 90 degrees
  // so (x, y) will be now (y, x)
  m_editor->processInput(vertexRadius);

  // render a window showing all the sidedefs and sectors
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
  for (const auto &objectId : m_editor->state->selection) {
    const auto object = m_editor->state->findObject(objectId);
    if (!object) continue;

    if (object->type == editor::EditorObjectType::VERTEX) {
      drawSelectedVertexPopup(objectId);
    } else if (object->type == editor::EditorObjectType::LINEDEF) {
      drawSelectedLinePopup(objectId);
    }
  }

  // draw line creation preview

  if (m_editor->state->isCreatingLine) {
    const auto &startVertex = m_editor->state->findVertex(m_editor->state->lineStartVertexId);

    const ImVec2 mousePos = ImGui::GetMousePos();

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddLine(startVertex.toImVec2(), mousePos, g_hoverColor, thickness);
  }

  ImGui::End();

  endFrame();
}


void ImguiRenderer::drawSidedefsWindow() const
{
  ImGui::Begin("SideDefs");

  if (ImGui::Button("Create sidedef")) {
    // create new sidedef
    m_editor->state->level->sidedefs.emplace_back(-1, 0, 0);
  }

  for (size_t i = 0; i < m_editor->state->level->sidedefs.size(); i++) {
    auto &sideDef = m_editor->state->level->sidedefs[i];
    ImGui::PushID(i);

    ImGui::Text("Sidedef: %d", i);

    int sectorId = sideDef.sectorId;
    int xOffset = sideDef.xOffset;
    int yOffset = sideDef.yOffset;

    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("Sector id", &sectorId)) {
      // update sector id
      sideDef.sectorId = static_cast<int16_t>(sectorId);
    }

    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("Texture offset x", &xOffset)) {
      // update texture offset x
      sideDef.xOffset = static_cast<int16_t>(xOffset);
    }
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("Texture offset y", &yOffset)) {
      // update texture offset y
      sideDef.yOffset = static_cast<int16_t>(yOffset);
    }
    float color[3] = { ImGui::ColorConvertU32ToFloat4(sideDef.color).x,
      ImGui::ColorConvertU32ToFloat4(sideDef.color).y,
      ImGui::ColorConvertU32ToFloat4(sideDef.color).z };

    if (ImGui::ColorPicker3("Sidedef color", color)) {
      // update sidedef color
      sideDef.color = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], 1.0f));
    }

    ImGui::NewLine();
    ImGui::PopID();
  }

  ImGui::End();
}// namespace imguirenderer

void ImguiRenderer::drawSectorsWindow() const
{

  ImGui::Begin("Sectors");

  if (ImGui::Button("Create sector")) { m_editor->state->level->sectors.emplace_back(); }

  for (uint32_t sectorId = 0; sectorId < m_editor->state->level->sectors.size(); sectorId++) {
    auto &sector = m_editor->state->findSector(sectorId);
    ImGui::PushID(sectorId);

    ImGui::Text("Sector: %d", sectorId);
    ImGui::SetNextItemWidth(100);
    int floorHeight = sector.floorHeight;
    int ceilingHeight = sector.ceilingHeight;
    if (ImGui::InputInt("Sector floor height", &floorHeight)) {
      sector.floorHeight = static_cast<int16_t>(floorHeight);
    }
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("Sector ceiling height", &ceilingHeight)) {
      sector.ceilingHeight = static_cast<int16_t>(ceilingHeight);
    }


    ImGui::NewLine();
    ImGui::PopID();
  }

  ImGui::End();
}

void ImguiRenderer::drawConnectedLineDefs(const std::vector<uint32_t> &connectedLineDefs,
  const editor::EditorLineDef &ld,
  const ImVec2 startDragged,
  const ImVec2 endDragged,
  ImDrawList *drawList,
  const float_t thickness) const
{
  for (auto &otherLdObjectId : connectedLineDefs) {
    const auto otherLdIndex = editor::getObjectIndex(otherLdObjectId);
    const auto &otherLd = m_editor->state->level->linedefs[otherLdIndex];
    if (otherLd.selected) continue;

    auto otherStart = m_editor->state->findVertex(otherLd.start);
    auto otherEnd = m_editor->state->findVertex(otherLd.end);

    if (otherLd.start == ld.start) {
      drawList->AddLine(startDragged, otherEnd.toImVec2(), g_defaultColor, thickness);
    } else if (otherLd.end == ld.start) {
      drawList->AddLine(startDragged, otherStart.toImVec2(), g_defaultColor, thickness);
    } else if (otherLd.start == ld.end) {
      drawList->AddLine(endDragged, otherEnd.toImVec2(), g_defaultColor, thickness);
    } else if (otherLd.end == ld.end) {
      drawList->AddLine(endDragged, otherStart.toImVec2(), g_defaultColor, thickness);
    }
  }
}

void ImguiRenderer::drawSelectedVertex(const uint32_t objectId,
  const float_t vertexRadius,
  const float_t thickness,
  ImDrawList *drawList) const
{
  const auto index = editor::getObjectIndex(objectId);
  const auto &v = m_editor->state->findVertex(index);

  const ImVec2 dragged(static_cast<float_t>(v.x) + m_editor->state->draggingOffset.x,
    static_cast<float_t>(v.y) + m_editor->state->draggingOffset.y);

  drawList->AddCircleFilled(dragged, vertexRadius, g_selectedColor);

  // draw every linedef connected to this vertex
  for (auto &ld : m_editor->state->level->linedefs) {
    if (ld.start == index || ld.end == index) {
      const auto otherVertexIndex = ld.start == index ? ld.end : ld.start;
      auto &otherVertex = m_editor->state->findVertex(otherVertexIndex);

      drawList->AddLine(dragged, otherVertex.toImVec2(), g_selectedColor, thickness);
    }
  }
}
void ImguiRenderer::drawSelectedLineDef(const uint32_t objectId,
  const float_t vertexRadius,
  const float_t thickness,
  ImDrawList *drawList) const
{
  const auto index = editor::getObjectIndex(objectId);
  auto &ld = m_editor->state->findLinedef(index);
  auto start = m_editor->state->findVertex(ld.start);
  auto end = m_editor->state->findVertex(ld.end);

  ImVec2 startDragged(static_cast<float_t>(start.x) + m_editor->state->draggingOffset.x,
    static_cast<float_t>(start.y) + m_editor->state->draggingOffset.y);
  ImVec2 endDragged(static_cast<float_t>(end.x) + m_editor->state->draggingOffset.x,
    static_cast<float_t>(end.y) + m_editor->state->draggingOffset.y);

  drawList->AddLine(startDragged, endDragged, g_selectedColor, thickness);

  drawList->AddCircleFilled(startDragged, vertexRadius, g_defaultColor);
  drawList->AddCircleFilled(endDragged, vertexRadius, g_defaultColor);

  // find all the linedefs connected to end/start and draw them to this line
  drawConnectedLineDefs(start.connectedLineDefs, ld, startDragged, endDragged, drawList, thickness);
  drawConnectedLineDefs(end.connectedLineDefs, ld, startDragged, endDragged, drawList, thickness);
}
void ImguiRenderer::drawSelection(const float_t vertexRadius, const float_t thickness, ImDrawList *drawList) const
{
  for (const uint32_t objectId : m_editor->state->selection) {
    const auto object = m_editor->state->findObject(objectId);
    if (!object) continue;

    switch (object->type) {
    case editor::EditorObjectType::LINEDEF: {
      drawSelectedLineDef(objectId, vertexRadius, thickness, drawList);
      break;
    }
    case editor::EditorObjectType::VERTEX: {
      drawSelectedVertex(objectId, vertexRadius, thickness, drawList);
      break;
    }
    default:
      throw std::runtime_error("Selected an unexpected/unknown type.");
    }
  }
}

void ImguiRenderer::drawUnselectedVertices(const float_t vertexRadius, ImDrawList *drawList) const
{
  for (auto &v : m_editor->state->level->vertices) {
    // selected vertices are processed separately
    if (v.selected || v.isAnyConnectedLineDefSelected(*m_editor->state)) continue;

    ImU32 color;

    if (v.hovered) {
      color = g_hoverColor;
    } else {
      color = g_defaultColor;
    }

    drawList->AddCircleFilled(v.toImVec2(), vertexRadius, color);
  }
}
void ImguiRenderer::drawUnselectedLineDefs(const float_t thickness, ImDrawList *drawList) const
{
  for (const auto &ld : m_editor->state->level->linedefs) {
    auto startVertex = m_editor->state->findVertex(ld.start);
    auto endVertex = m_editor->state->findVertex(ld.end);

    // selected linedefs are processed separately
    if (startVertex.selected || endVertex.selected || ld.selected
        || startVertex.isAnyConnectedLineDefSelected(*m_editor->state)
        || endVertex.isAnyConnectedLineDefSelected(*m_editor->state)) {
      continue;
    }

    ImVec2 start = startVertex.toImVec2();
    ImVec2 end = endVertex.toImVec2();

    ImU32 color;

    if (ld.hovered) {
      color = g_hoverColor;
    } else {
      color = g_defaultColor;
    }

    // tint a bit the color of the portal linedef
    if (ld.backSideDef != -1) { color -= 0x32323200; }

    drawList->AddLine(start, end, color, thickness);
  }
}
void ImguiRenderer::drawMapOutlines(const float_t vertexRadius, const float_t thickness) const
{
  ImDrawList *drawList = ImGui::GetWindowDrawList();

  // draw outlines
  drawUnselectedLineDefs(thickness, drawList);
  // draw vertices
  drawUnselectedVertices(vertexRadius, drawList);
  // draw selected vertices/linedefs
  drawSelection(vertexRadius, thickness, drawList);
}

void ImguiRenderer::showVertexRLineCreation(const ImVec2 &mousePos, bool &isOpen) const
{
  ImGui::SetNextWindowPos(mousePos);
  ImGui::Begin("Vertex/Line creation popup");
  const bool vertexCreation = ImGui::Button("Create Vertex");
  const bool connectedVertexCreation = ImGui::Button("Create Connected Vertex");
  const bool lineCreation = ImGui::Button("Create Line");

  if (vertexCreation) {
    m_editor->addVertex(static_cast<int16_t>(mousePos.x), static_cast<int16_t>(mousePos.y));
    isOpen = false;
  } else if (connectedVertexCreation) {
    // TODO:
  } else if (lineCreation) {
    // TODO:
  }

  ImGui::End();
}

void ImguiRenderer::drawSelectedLinePopup(const uint32_t lineId) const
{
  auto index = editor::getObjectIndex(lineId);
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

  ImGui::NewLine();

  const std::string frontSideDefLabel = fmt::format(":Front SideDef ##LDSidedef{0}", ld.start);
  const std::string backSideDefLabel = fmt::format(":Back SideDef ##LDSidedef{0}", ld.end);

  createSelect(frontSideDefLabel.c_str(), m_editor->state->level->sidedefs, ld.frontSideDef);
  createSelect(backSideDefLabel.c_str(), m_editor->state->level->sidedefs, ld.backSideDef, true);

  if (ImGui::Button("Delete")) { editor::EditorLineDef::remove(*m_editor->state, index); }

  ImGui::NewLine();
  ImGui::NewLine();

  ImGui::PopID();
  ImGui::End();
}

void ImguiRenderer::drawSelectedVertexPopup(const uint32_t selectedId) const
{
  const uint32_t index = editor::getObjectIndex(selectedId);

  const auto &vertex = m_editor->state->findVertex(index);
  const std::string windowTitle = "Vertex params: " + std::to_string(index) + "###Vertex" + std::to_string(index);
  ImGui::Begin(windowTitle.c_str());

  ImGui::PushID(std::to_string(selectedId).c_str());

  for (const auto &ldObjectId : vertex.connectedLineDefs) {
    const auto ldIndex = editor::getObjectIndex(ldObjectId);

    ImGui::Text("Connected LineDef ID: %u", ldIndex);
  }

  if (ImGui::Button("Create Connected Line")) { m_editor->drawConnectedLine(index); }
  if (ImGui::Button("Delete")) { editor::EditorVertex::remove(*m_editor->state, index); }

  ImGui::PopID();
  ImGui::End();
}

void ImguiRenderer::createSelect(const char *label,
  const std::vector<gameloop::SideDef> &sidedefs,
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

constexpr ImVec2 ImguiRenderer::scale(const ImVec2 vec, const float_t scaleFactor)
{ return { scaleFactor * vec.x, scaleFactor * vec.y }; }


}// namespace imguirenderer
