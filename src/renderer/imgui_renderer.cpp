#include "imgui_renderer.h"
#include "editor.h"
#include "editor_input_handler.h"
#include "math_utils.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <memory>
#include <string>
#include <vector>

namespace imguirenderer {

ImguiRenderer::ImguiRenderer(sdl_window::SdlWindow &sdlWindow, gameloop::Level &level) : m_sdlWindow(sdlWindow)
{
  float_t mainScale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);

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

void ImguiRenderer::endFrame()
{
  ImGui::Render();
  ImGuiIO &currentIo = ImGui::GetIO();
  SDL_RenderSetScale(
    m_sdlWindow.getRenderer(), currentIo.DisplayFramebufferScale.x, currentIo.DisplayFramebufferScale.y);

  SDL_RenderClear(m_sdlWindow.getRenderer());
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_sdlWindow.getRenderer());
  SDL_RenderPresent(m_sdlWindow.getRenderer());
}

void ImguiRenderer::render()
{

  // Start the Dear ImGui frame
  startFrame();

  ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);

  // 2. Set flags to make it invisible and non-interactive
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground
                           | ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::Begin("HUDOverlay", nullptr, flags);

  // 3. Draw your "screen-level" elements
  ImGui::TextColored(ImVec4(1, 1, 0, 1), "FPS: %.1f", io.Framerate);

  // for rendering the map of the level will be used a coordinate system which is rotated by 90 degrees
  // so (x, y) will be now (y, x)
  m_editor->processInput();

  // suggest creating a new line/vertex
  if (m_editor->state->renderOptionsWindow) {
    showVertexRLineCreation(m_editor->state->optionsWindowPos, m_editor->state->renderOptionsWindow);
  }

  drawMapOutlines();

  for (auto &id : m_editor->state->selection) {
    if (m_editor->state->findVertex(id)) {
      drawSelectedVertexPopup(id);
    } else if (m_editor->state->findLinedef(id)) {
      drawSelectedLinePopup(id);
    }
  }

  ImGui::End();

  endFrame();
}

void ImguiRenderer::drawMapOutlines()
{
  static ImU32 s_defaultColor = IM_COL32(255, 255, 255, 255);
  static ImU32 s_selectedColor = IM_COL32(0, 0, 255, 255);
  static ImU32 s_hoverColor = IM_COL32(255, 200, 0, 255);
  static float_t s_thickness = 2.0f;
  static float_t s_vertexRadius = 4.0f;

  ImDrawList *drawList = ImGui::GetWindowDrawList();

  static auto toImVec2 = [&](const editor::EditorVertex *v) -> ImVec2 {
    if (!v) return ImVec2(0.0f, 0.0f);
    return ImVec2(v->x, v->y);
  };

  // draw outlines
  for (auto &ld : m_editor->state->level->linedefs) {
    ImVec2 start = toImVec2(m_editor->state->findVertex(ld.start));
    ImVec2 end = toImVec2(m_editor->state->findVertex(ld.end));
    ImU32 color = s_defaultColor;

    if (ld.selected) {
      color = s_selectedColor;
    } else if (ld.hovered) {
      color = s_hoverColor;
    } else {
      color = s_defaultColor;
    }

    // tint a bit the color of the portal linedef
    if (ld.backSidedef != -1) { color -= 0x32323200; }

    drawList->AddLine(start, end, color, s_thickness);
  }

  // draw vertices
  for (auto &v : m_editor->state->level->vertices) {
    ImVec2 pos = toImVec2(&v);
    ImU32 color;

    if (v.selected) {
      color = s_selectedColor;
    } else if (v.hovered) {
      color = s_hoverColor;
    } else {
      color = s_defaultColor;
    }

    drawList->AddCircleFilled(pos, s_vertexRadius, color);
  }
}

void ImguiRenderer::showVertexRLineCreation(const ImVec2 &mousePos, bool &isOpen)
{
  ImGui::SetNextWindowPos(mousePos);
  ImGui::Begin("Vertex/Line creation popup");
  bool vertexCreation = ImGui::Button("Create Vertex");
  bool connectedVertexCreation = ImGui::Button("Create Connected Vertex");
  bool lineCreation = ImGui::Button("Create Line");

  if (vertexCreation) {
    gameloop::Vertex v = math_utils::toCenterCoordinates(
      math_utils::convertImVec2IntoVertex(mousePos), m_sdlWindow.width, m_sdlWindow.height);

    m_editor.get()->addVertex(-1, v);
    isOpen = false;
  } else if (connectedVertexCreation) {
    // TODO:
  } else if (lineCreation) {
    // TODO:
  }

  ImGui::End();
}

void ImguiRenderer::drawSelectedLinePopup(uint32_t selectedIndex)
{
  if (selectedIndex == 0) return;

  auto ld = m_editor->state->findLinedef(selectedIndex);
  if (!ld) return;

  auto start = m_editor->state->findVertex(ld->start);
  auto end = m_editor->state->findVertex(ld->end);

  // render popup of linedef parameters
  std::string windowTitle = "Linedef params " + std::to_string(selectedIndex) + "###LinedefParams";
  ImGui::Begin(windowTitle.c_str());

  ImGui::SetNextItemWidth(80);
  ImGui::InputInt(": Start X", &start->x);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  ImGui::InputInt(": Start Y", &start->y);

  ImGui::NewLine();

  ImGui::SetNextItemWidth(80);
  ImGui::InputInt("End X: ", &end->x);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  ImGui::InputInt("End Y: ", &end->y);

  ImGui::NewLine();

  createSelect("Front Sidedef: ", m_editor->state->level->sidedefs, ld->frontSidedef);
  createSelect("Back Sidedef: ", m_editor->state->level->sidedefs, ld->backSidedef, true);

  ImGui::End();
}

void ImguiRenderer::drawSelectedVertexPopup(uint32_t selectedIndex)
{
  if (selectedIndex == 0) return;
  auto vertex = m_editor->state->findVertex(selectedIndex);
  if (!vertex) return;

  std::string windowTitle = "Vertex params: " + std::to_string(selectedIndex) + "###VertexParams";
  ImGui::Begin(windowTitle.c_str());

  bool isDeleted = ImGui::Button("Delete");

  if (isDeleted) {
    // Remove all linedefs that reference this vertex (iterate backwards to avoid index issues)
    for (int32_t i = static_cast<int32_t>(m_editor->state->level->linedefs.size()) - 1; i >= 0; i--) {
      if (m_editor->state->level->linedefs[i].start == selectedIndex
          || m_editor->state->level->linedefs[i].end == selectedIndex) {
        m_editor->state->level->linedefs[i] = m_editor->state->level->linedefs.back();
        m_editor->state->level->linedefs.pop_back();
      }
    }

    // Get the index of the vertex that will be moved (the last one)
    // uint32_t lastVertexId = m_editor->state->level->vertices.back().id;

    // Update all linedef references from lastVertexIndex to selectedIndex (if different)
    // if (selectedIndex != lastVertexId) {
    //   for (auto &linedef : m_editor->state->level->linedefs) {
    //     if (linedef.start == lastVertexId) { linedef.start = selectedIndex; }
    //     if (linedef.end == lastVertexId) { linedef.end = selectedIndex; }
    //   }
    // }

    // Remove the vertex using swap-and-pop
    *vertex = m_editor->state->level->vertices.back();
    m_editor->state->level->vertices.pop_back();
  }

  ImGui::End();
}

void ImguiRenderer::createSelect(const char *label,
  const std::vector<gameloop::SideDef> &sidedefs,
  int16_t &currentItem,
  bool hasReset)
{
  if (ImGui::BeginCombo(label, std::to_string(currentItem).c_str())) {
    if (hasReset) {
      bool isReset = currentItem == -1;

      const char *resetLabel = "-1";
      if (ImGui::Selectable(resetLabel, isReset)) { currentItem = -1; }
    }

    for (uint16_t i = 0; i < sidedefs.size(); i++) {
      bool is_selected = (currentItem == i);
      std::string optionLabel = std::to_string(i);

      if (ImGui::Selectable(optionLabel.c_str(), is_selected)) { currentItem = i; }

      // Set the initial focus when opening the combo (scrolling to selection)
      if (is_selected) { ImGui::SetItemDefaultFocus(); }
    }
    ImGui::EndCombo();
  }
}

constexpr ImVec2 ImguiRenderer::scale(ImVec2 vec, float_t scaleFactor)
{
  return { scaleFactor * vec.x, scaleFactor * vec.y };
}


}// namespace imguirenderer
