#include "imgui_renderer.h"
#include "editor.h"
#include "editor_input_handler.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <algorithm>
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

  for (auto &objectId : m_editor->state->selection) {
    auto object = m_editor->state->findObject(objectId);
    if (!object) continue;

    if (object->type == editor::EditorObjectType::VERTEX) {
      drawSelectedVertexPopup(objectId);
    } else if (object->type == editor::EditorObjectType::LINEDEF) {
      drawSelectedLinePopup(objectId);
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
  float_t s_thickness = 2.0f * m_editor->state->canvasZoom;
  float_t s_vertexRadius = 4.0f * m_editor->state->canvasZoom;

  ImDrawList *drawList = ImGui::GetWindowDrawList();

  // draw outlines
  for (auto &ld : m_editor->state->level->linedefs) {
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
    // selected vertices are processed separately
    if (v.selected || v.isAnyConnectedLineDefSelected(*m_editor->state)) continue;

    ImU32 color;

    if (v.hovered) {
      color = s_hoverColor;
    } else {
      color = s_defaultColor;
    }

    drawList->AddCircleFilled(v.toImVec2(), s_vertexRadius, color);
  }

  // draw selected vertices/linedefs
  for (uint32_t objectId : m_editor->state->selection) {
    // O(1) lookup for an object
    auto object = m_editor->state->findObject(objectId);
    if (!object) continue;

    switch (object->type) {
    case editor::EditorObjectType::LINEDEF: {
      auto index = editor::getObjectIndex(objectId);
      auto &ld = m_editor->state->findLinedef(index);
      auto start = m_editor->state->findVertex(ld.start);
      auto end = m_editor->state->findVertex(ld.end);

      ImVec2 startDragged(start.x + m_editor->state->draggingOffset.x, start.y + m_editor->state->draggingOffset.y);
      ImVec2 endDragged(end.x + m_editor->state->draggingOffset.x, end.y + m_editor->state->draggingOffset.y);

      drawList->AddLine(startDragged, endDragged, s_selectedColor, s_thickness);

      drawList->AddCircleFilled(startDragged, s_vertexRadius, s_defaultColor);
      drawList->AddCircleFilled(endDragged, s_vertexRadius, s_defaultColor);

      // find all the linedefs connected to end/start and draw them to this line
      for (auto &otherLdObjectId : start.connectedLineDefs) {
        auto otherLdIndex = editor::getObjectIndex(otherLdObjectId);
        auto &otherLd = m_editor->state->level->linedefs[otherLdIndex];
        if (otherLd.selected) continue;

        auto otherStart = m_editor->state->findVertex(otherLd.start);
        auto otherEnd = m_editor->state->findVertex(otherLd.end);

        if (otherLd.start == ld.start) {
          drawList->AddLine(startDragged, otherEnd.toImVec2(), s_defaultColor, s_thickness);
        } else if (otherLd.end == ld.start) {
          drawList->AddLine(startDragged, otherStart.toImVec2(), s_defaultColor, s_thickness);
        } else if (otherLd.start == ld.end) {
          drawList->AddLine(endDragged, otherEnd.toImVec2(), s_defaultColor, s_thickness);
        } else if (otherLd.end == ld.end) {
          drawList->AddLine(endDragged, otherStart.toImVec2(), s_defaultColor, s_thickness);
        }
      }

      for (auto &otherLdObjectId : end.connectedLineDefs) {
        auto otherLdIndex = editor::getObjectIndex(otherLdObjectId);
        auto &otherLd = m_editor->state->level->linedefs[otherLdIndex];
        if (otherLd.selected) continue;

        auto otherStart = m_editor->state->findVertex(otherLd.start);
        auto otherEnd = m_editor->state->findVertex(otherLd.end);

        if (otherLd.start == ld.start) {
          drawList->AddLine(startDragged, otherEnd.toImVec2(), s_defaultColor, s_thickness);
        } else if (otherLd.end == ld.start) {
          drawList->AddLine(startDragged, otherStart.toImVec2(), s_defaultColor, s_thickness);
        } else if (otherLd.start == ld.end) {
          drawList->AddLine(endDragged, otherEnd.toImVec2(), s_defaultColor, s_thickness);
        } else if (otherLd.end == ld.end) {
          drawList->AddLine(endDragged, otherStart.toImVec2(), s_defaultColor, s_thickness);
        }
      }

      break;
    }
    case editor::EditorObjectType::VERTEX: {
      auto index = editor::getObjectIndex(objectId);
      auto &v = m_editor->state->findVertex(index);

      ImVec2 dragged(v.x + m_editor->state->draggingOffset.x, v.y + m_editor->state->draggingOffset.y);

      drawList->AddCircleFilled(dragged, s_vertexRadius, s_selectedColor);

      // draw every linedef connected to this vertex

      for (auto &ld : m_editor->state->level->linedefs) {
        if (ld.selected)
          continue;
        else if (ld.start == index || ld.end == index) {
          auto otherVertexIndex = (ld.start == index) ? ld.end : ld.start;
          auto &otherVertex = m_editor->state->findVertex(otherVertexIndex);

          drawList->AddLine(dragged, otherVertex.toImVec2(), s_selectedColor, s_thickness);
        }
      }
      break;
    }
    default:
      throw std::runtime_error("Selected an unexpected/unknown type.");
      break;
    }
  }
}

// void ImguiRenderer::updateAfterDragging()
// {
//   for (uint32_t id : m_editor->state->selection) {
//     auto object = m_editor->state->findObject(id);

//     uint32_t index = editor::getObjectIndex(id);
//     editor::EditorObjectType type = editor::getObjectType(id);

//     if (object) {
//       switch (type) {
//       case editor::EditorObjectType::VERTEX: {
//         auto &vertex = m_editor->state->findVertex(index);
//         auto cmd = std::make_unique<commands::MoveVertexCommand>(index,
//           vertex,
//           editor::EditorVertex(
//             vertex.x + m_editor->state->draggingOffset.x, vertex.y + m_editor->state->draggingOffset.y));

//         m_editor->executeCommand(std::move(cmd));
//         break;
//       }
//       case editor::EditorObjectType::LINEDEF: {
//         auto &line = m_editor->state->findLinedef(index);
//         auto &start = m_editor->state->findVertex(line.start), &end = m_editor->state->findVertex(line.end);
//         auto cmd = std::make_unique<commands::MoveLineDefCommand>(
//           index, start, end, start + m_editor->state->draggingOffset, end + m_editor->state->draggingOffset);

//         m_editor->executeCommand(std::move(cmd));
//         break;
//       }
//       default:
//         break;
//       }
//     }
//   }
// }

void ImguiRenderer::showVertexRLineCreation(const ImVec2 &mousePos, bool &isOpen)
{
  ImGui::SetNextWindowPos(mousePos);
  ImGui::Begin("Vertex/Line creation popup");
  bool vertexCreation = ImGui::Button("Create Vertex");
  bool connectedVertexCreation = ImGui::Button("Create Connected Vertex");
  bool lineCreation = ImGui::Button("Create Line");

  if (vertexCreation) {
    m_editor.get()->addVertex(-1, static_cast<int16_t>(mousePos.x), static_cast<int16_t>(mousePos.y));
    isOpen = false;
  } else if (connectedVertexCreation) {
    // TODO:
  } else if (lineCreation) {
    // TODO:
  }

  ImGui::End();
}

void ImguiRenderer::drawSelectedLinePopup(uint32_t objectId)
{
  auto index = editor::getObjectIndex(objectId);
  auto &ld = m_editor->state->findLinedef(index);

  auto &start = m_editor->state->findVertex(ld.start);
  auto &end = m_editor->state->findVertex(ld.end);

  // render popup of linedef parameters
  std::string windowTitle = "Linedef params " + std::to_string(index) + "###LinedefParams";
  ImGui::Begin(windowTitle.c_str());

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

  createSelect("Front Sidedef: ", m_editor->state->level->sidedefs, ld.frontSidedef);
  createSelect("Back Sidedef: ", m_editor->state->level->sidedefs, ld.backSidedef, true);

  ImGui::End();
}

void ImguiRenderer::drawSelectedVertexPopup(uint32_t objectId)
{
  auto index = editor::getObjectIndex(objectId);
  auto &vertex = m_editor->state->findVertex(index);

  std::string windowTitle = "Vertex params: " + std::to_string(index) + "###VertexParams";
  ImGui::Begin(windowTitle.c_str());

  bool isDeleted = ImGui::Button("Delete");

  if (isDeleted) {
    auto &selection = m_editor->state->selection;
    auto removeSelectionId = [&](uint32_t id) {
      selection.erase(
        std::remove_if(selection.begin(), selection.end(), [&](uint32_t selectedId) { return selectedId == id; }),
        selection.end());
    };
    auto replaceSelectionId = [&](uint32_t fromId, uint32_t toId) {
      for (auto &selectedId : selection) {
        if (selectedId == fromId) selectedId = toId;
      }
    };

    // Remove all linedefs that reference this vertex (iterate backwards to avoid index issues)
    for (int i = (int)vertex.connectedLineDefs.size() - 1; i >= 0; i--) {
      if (i < 0) break;

      uint32_t ldId = editor::getObjectIndex(vertex.connectedLineDefs[i]);
      auto &ld = m_editor->state->level->linedefs[ldId];

      if (ld.start == index) {
        auto &vEnd = m_editor->state->findVertex(ld.end);

        vEnd.connectedLineDefs.erase(
          std::remove_if(vEnd.connectedLineDefs.begin(),
            vEnd.connectedLineDefs.end(),
            [&ldId](const auto &ldObjectId) { return editor::getObjectIndex(ldObjectId) == ldId; }),
          vEnd.connectedLineDefs.end());
      } else if (ld.end == index) {
        auto &vStart = m_editor->state->findVertex(ld.start);

        vStart.connectedLineDefs.erase(
          std::remove_if(vStart.connectedLineDefs.begin(),
            vStart.connectedLineDefs.end(),
            [&ldId](const auto &ldObjectId) { return editor::getObjectIndex(ldObjectId) == ldId; }),
          vStart.connectedLineDefs.end());
      }

      uint32_t lastLdIndex = static_cast<uint32_t>(m_editor->state->level->linedefs.size() - 1);
      
      if (ldId != lastLdIndex) {
        // Update connected vertices to point to the new linedef position
        auto &lastLd = m_editor->state->level->linedefs.back();
        auto &vStart = m_editor->state->findVertex(lastLd.start);
        auto &vEnd = m_editor->state->findVertex(lastLd.end);

        // Update indices in connected vertices
        for (auto &ldObjectId : vStart.connectedLineDefs) {
          if (editor::getObjectIndex(ldObjectId) == lastLdIndex) {
            ldObjectId = editor::makeObjectId(editor::EditorObjectType::LINEDEF, ldId);
            break;
          }
        }
        for (auto &ldObjectId : vEnd.connectedLineDefs) {
          if (editor::getObjectIndex(ldObjectId) == lastLdIndex) {
            ldObjectId = editor::makeObjectId(editor::EditorObjectType::LINEDEF, ldId);
            break;
          }
        }

        ld = lastLd;
      }

      m_editor->state->level->linedefs.pop_back();
    }

    // Remove the vertex using swap-and-pop
    uint32_t lastVertexIndex = static_cast<uint32_t>(m_editor->state->level->vertices.size() - 1);
    removeSelectionId(editor::makeObjectId(editor::EditorObjectType::VERTEX, index));

    if (index != lastVertexIndex) {
      auto &lastVertex = m_editor->state->level->vertices.back();

      for (uint32_t ldObjectId : lastVertex.connectedLineDefs) {
        auto ldIndex = editor::getObjectIndex(ldObjectId);
        auto &ld = m_editor->state->findLinedef(ldIndex);

        if (ld.start == lastVertexIndex) ld.start = index;
        if (ld.end == lastVertexIndex) ld.end = index;
      }

      replaceSelectionId(editor::makeObjectId(editor::EditorObjectType::VERTEX, lastVertexIndex),
        editor::makeObjectId(editor::EditorObjectType::VERTEX, index));

      vertex = lastVertex;
    }
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
