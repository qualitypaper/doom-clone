#include "editor_renderer.h"

#include "commands.h"
#include "editor.h"
#include "math_utils.h"

#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <algorithm>
#include <fmt/core.h>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

static ImU32 g_defaultColor = IM_COL32(255, 255, 255, 255);
static ImU32 g_selectedColor = IM_COL32(0, 0, 255, 255);
static ImU32 g_hoverColor = IM_COL32(255, 200, 0, 255);

static float g_defaultVertexRadius = 4.0f;
static float g_defaultLinedefThickness = 2.0f;

EditorRenderer::EditorRenderer(SdlWindow &sdlWindow,
  Level &level,
  const uint16_t _levelNum,
  const uint16_t _numOfLevels)
  : m_sdlWindow(sdlWindow)
{
  const float_t mainScale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);

  IMGUI_CHECKVERSION();
  ImGuiContext *context = ImGui::CreateContext();
  ImGui::SetCurrentContext(context);

  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;// Enable Keyboard Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup scaling
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(mainScale);// Bake a fixed style scale.
  style.FontScaleDpi = mainScale;// Set initial font scale.

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForSDLRenderer(sdlWindow.getWindow(), sdlWindow.getRenderer());
  ImGui_ImplSDLRenderer2_Init(sdlWindow.getRenderer());

  this->m_editor = std::make_unique<Editor>(level, _levelNum, _numOfLevels, sdlWindow.width, sdlWindow.height);
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
  if (!m_editor->state->isCreatingLine)
    return;

  const EditorVertex &startVertex = m_editor->state->findVertex(m_editor->state->lineStartVertexId);
  const ImVec2 startTransformed = m_editor->TransformVertex(startVertex);

  const ImVec2 mousePos = ImGui::GetMousePos();

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddLine(startTransformed, mousePos, g_hoverColor, thickness);
}

void EditorRenderer::drawPopupsForSelectedObjects() const
{
  if (m_editor->state->selection.empty())
    return;

  // used deffered deletion logic in order not to break selection iteration
  std::vector<uint32_t> idsToRemove;

  for (const uint32_t objectId : m_editor->state->selection) {
    const auto object = m_editor->state->findObject(objectId);
    if (!object)
      continue;

    bool toRemove = false;

    if (object->type == EditorObjectType::VERTEX) {
      toRemove = drawSelectedVertexPopup(objectId);
    } else if (object->type == EditorObjectType::LINEDEF) {
      toRemove = drawSelectedLinePopup(objectId);
    }

    if (toRemove) {
      idsToRemove.push_back(objectId);
    }
  }

  // remove requested ids
  for (const uint32_t id : idsToRemove) {
    auto type = getObjectType(id);

    // TODO: make delete commands in order to split objects deletion correctly
    if (type == EditorObjectType::VERTEX) {
      EditorVertex::remove(*m_editor->state, id);
    } else if (type == EditorObjectType::LINEDEF) {
      EditorLineDef::remove(*m_editor->state, id);
    }
  }
}

void EditorRenderer::render() const
{
  SDL_SetRenderDrawColor(m_sdlWindow.getRenderer(), 0, 0, 0, 255);

  // Start the Dear ImGui frame
  startFrame();
  m_editor->resetStateFrame();

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
  m_editor->ProcessInput(vertexRadius);

  // suggest creating a new line/vertex
  if (m_editor->state->renderOptionsWindow) {
    showVertexRLineCreation(m_editor->state->optionsWindowPos, m_editor->state->renderOptionsWindow);
  }

  m_editor->TransformVertices();

  drawCoordinatesCenter(vertexRadius);

  // render a window showing all the sectors
  drawSectorsWindow();

  // render a window showing all sidedefs
  drawSidedefsWindow();

  // draw map outlines
  drawMapOutlines(vertexRadius, thickness);

  // draw line creation preview
  drawLinePreview(thickness);

  // draw popups for selected objects
  drawPopupsForSelectedObjects();

  // draw block selection indication
  drawBlockSelection();

  // draw level selection select
  drawLevelSelection();

  ImGui::End();

  endFrame();
}

void EditorRenderer::drawLevelSelection() const
{
  ImGui::SetNextWindowPos(ImVec2(10, 10));
  ImGui::SetNextWindowSize(ImVec2(150, 0));

  ImGui::Begin("LevelSelection",
    nullptr,
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

  // TODO: rewrite createSelect to use a index based for loop instead of storing indices in a seperate array
  std::vector<std::uint16_t> levelOptions;
  levelOptions.reserve(m_editor->state->numOfLevels);

  for (uint16_t i = 0; i < m_editor->state->numOfLevels; i++) {
    levelOptions.emplace_back(i + 1);
  }

  uint16_t currentLevelOption = m_editor->state->level->levelNum;

  createSelect(
    "Select level",
    levelOptions,
    currentLevelOption,
    [this, &currentLevelOption](const uint16_t selectedOption) {
      currentLevelOption = selectedOption;

      m_editor->changeLevel(selectedOption);
    },
    [this]() { m_editor->addEmptyLevel(); });

  ImGui::End();
}

void EditorRenderer::drawBlockSelection() const
{
  if (!m_editor->state->isBlockSelecting)
    return;

  const auto drawList = ImGui::GetWindowDrawList();
  const ImVec2 blockSelectionStart = m_editor->state->blockSelectionStart;
  const ImVec2 blockSelectionEnd = { blockSelectionStart.x + m_editor->state->blockSelectionOffset.x,
    blockSelectionStart.y + m_editor->state->blockSelectionOffset.y };

  const float minX = std::min(blockSelectionStart.x, blockSelectionEnd.x);
  const float maxX = std::max(blockSelectionStart.x, blockSelectionEnd.x);
  const float minY = std::min(blockSelectionStart.y, blockSelectionEnd.y);
  const float maxY = std::max(blockSelectionStart.y, blockSelectionEnd.y);

  drawList->AddRectFilled({ minX, minY }, { maxX, maxY }, IM_COL32(100, 100, 0, 150));
}

void EditorRenderer::drawSidedefsWindow() const
{
  ImGui::Begin("SideDefs");

  if (ImGui::Button("Create sidedef")) {
    // create new sidedef
    m_editor->state->level->sidedefs.emplace_back(-1, 0, 0);
  }

  constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH
    | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody
    | ImGuiTableFlags_ScrollY;// Allows the list to scroll if it exceeds window height

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

  if (ImGui::Button("Create sector")) {
    m_editor->state->level->sectors.emplace_back();
  }

  constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH
    | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody
    | ImGuiTableFlags_ScrollY;// Allows the list to scroll if it exceeds window height

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
void EditorRenderer::drawVertices(const float_t vertexRadius) const
{
  for (size_t i = 0; i < m_editor->state->level->vertices.size(); i++) {
    drawVertex(makeObjectId(EditorObjectType::VERTEX, i), vertexRadius);
  }
}

void EditorRenderer::drawArrowForLinedef(const float_t thickness,
  const uint32_t startVertexId,
  const uint32_t endVertexId,
  const ImU32 color) const
{
  static double s_arrowLength = 15;
  static double s_arrowAngle = -45;

  const ImVec2 startVec = m_editor->state->transformedVertices[getObjectIndex(startVertexId)];
  const ImVec2 endVec = m_editor->state->transformedVertices[getObjectIndex(endVertexId)];

  EditorVertex linedefDir{ endVec.x - startVec.x, endVec.y - startVec.y };
  linedefDir.normalize();

  const double angle = glm::acos(linedefDir.x / 180 * M_PI) * 180 / M_PI;

  const ImVec2 leftArrowDir = math_utils::rotateAroundX(linedefDir.toImVec2(), -angle + s_arrowAngle);
  const ImVec2 rightArrowDir = math_utils::rotateAroundX(linedefDir.toImVec2(), angle - s_arrowAngle);

  const float zoom = std::max(0.1f, m_editor->state->canvasZoom);

  const ImVec2 leftArrowEnd{ static_cast<float>(endVec.x + leftArrowDir.x * s_arrowLength * zoom),
    static_cast<float>(endVec.y + leftArrowDir.y * s_arrowLength * zoom) };
  const ImVec2 rightArrowEnd{ static_cast<float>(endVec.x + rightArrowDir.x * s_arrowLength * zoom),
    static_cast<float>(endVec.y + rightArrowDir.y * s_arrowLength * zoom) };

  ImDrawList *drawList = ImGui::GetWindowDrawList();

  drawList->AddLine(endVec, leftArrowEnd, color, thickness * zoom / 2.0f);
  drawList->AddLine(endVec, rightArrowEnd, color, thickness * zoom / 2.0f);
}

void EditorRenderer::drawLinedef(const EditorLineDef &ld, const float_t thickness) const
{

  ImU32 color;
  if (ld.selected) {
    color = g_selectedColor;
  } else if (ld.hovered) {
    color = g_hoverColor;
  } else {
    color = g_defaultColor;
  }

  // tint a bit the color of the portal linedef
  if (ld.backSideDef != -1) {
    color -= 0x32323200;
  }

  ImDrawList *drawList = ImGui::GetWindowDrawList();

  const ImVec2 &startVec = m_editor->state->transformedVertices[getObjectIndex(ld.start)];
  const ImVec2 &endVec = m_editor->state->transformedVertices[getObjectIndex(ld.end)];

  if (startVec.x < 0 || startVec.y < 0 || m_sdlWindow.width <= startVec.x || m_sdlWindow.height <= startVec.y)
    return;
  if (endVec.x < 0 || endVec.y < 0 || m_sdlWindow.width <= endVec.x || m_sdlWindow.height <= endVec.y)
    return;

  drawList->AddLine(startVec, endVec, color, thickness);

  // draw a small arrow showing the direction of the linedef
  drawArrowForLinedef(thickness, ld.start, ld.end, color);
}

void EditorRenderer::drawLinedefs(const float_t thickness) const
{
  for (const auto &ld : m_editor->state->level->linedefs) {
    drawLinedef(ld, thickness);
  }
}
void EditorRenderer::drawMapOutlines(const float_t vertexRadius, const float_t thickness) const
{
  // draw outlines
  drawLinedefs(thickness);
  // draw vertices
  drawVertices(vertexRadius);
}

void EditorRenderer::drawCoordinatesCenter(const float vertexRadius) const
{
  const EditorVertex center =
    math_utils::fromCenterCoordinates(EditorVertex(0, 0), m_sdlWindow.width, m_sdlWindow.height);
  const ImVec2 centerTransformed = m_editor->TransformVertex(center);

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddCircleFilled(centerTransformed, vertexRadius, IM_COL32(50, 0, 255, 255));
}

void EditorRenderer::showVertexRLineCreation(const ImVec2 &mousePos, bool &isOpen) const
{
  ImGui::SetNextWindowPos(mousePos);
  ImGui::Begin("Vertex/Line creation popup");
  const bool vertexCreation = ImGui::Button("Create Vertex");

  if (vertexCreation) {
    const int32_t x = mousePos.x - m_editor->state->scrollingOffset.x;
    const int32_t y = mousePos.y - m_editor->state->scrollingOffset.y;

    const ImVec2 zoomed = m_editor->zoomVertex(ImVec2(x, y), 1 / m_editor->state->canvasZoom);

    m_editor->addVertex(zoomed.x, zoomed.y);
    isOpen = false;
  }

  ImGui::End();
}

bool EditorRenderer::drawSelectedLinePopup(const uint32_t lineId) const
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
  ImGui::InputDouble(": Start X", &start.x);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  ImGui::InputDouble(": Start Y", &start.y);

  ImGui::NewLine();

  ImGui::SetNextItemWidth(80);
  ImGui::InputDouble("End X: ", &end.x);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  ImGui::InputDouble("End Y: ", &end.y);

  if (ImGui::Button("Swap")) {
    const uint32_t temp = ld.start;
    ld.start = ld.end;
    ld.end = temp;
  }

  ImGui::NewLine();

  const std::string frontSideDefLabel = fmt::format(":Front SideDef ##LDSidedef{0}", ld.start);
  const std::string backSideDefLabel = fmt::format(":Back SideDef ##LDSidedef{0}", ld.end);

  createSidedefSelect(frontSideDefLabel.c_str(), m_editor->state->level->sidedefs, ld.frontSideDef);
  createSidedefSelect(backSideDefLabel.c_str(), m_editor->state->level->sidedefs, ld.backSideDef, true);

  bool deleteRequested = false;

  if (ImGui::Button("Delete")) {
    deleteRequested = true;
    // EditorLineDef::remove(*m_editor->state, index);
  }

  ImGui::NewLine();
  ImGui::NewLine();

  ImGui::PopID();
  ImGui::End();

  return deleteRequested;
}

bool EditorRenderer::drawSelectedVertexPopup(const uint32_t selectedId) const
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

  if (ImGui::Button("Create Connected Line")) {
    m_editor->drawConnectedLine(selectedId);
  }

  bool deleteRequested = false;
  if (ImGui::Button("Delete")) {
    deleteRequested = true;
    // EditorVertex::remove(*m_editor->state, selectedId);
  }

  ImGui::PopID();
  ImGui::End();

  return deleteRequested;
}

void EditorRenderer::createSidedefSelect(const char *label,
  const std::vector<EditorSidedef> &sidedefs,
  int16_t &currentItem,
  const bool hasReset)
{
  if (ImGui::BeginCombo(label, std::to_string(currentItem).c_str())) {
    // handling -1 option separately
    if (hasReset || currentItem == -1) {
      const bool isReset = currentItem == -1;

      const auto resetLabel = "-1";
      if (ImGui::Selectable(resetLabel, isReset)) {
        currentItem = -1;
      }
    }

    for (size_t i = 0; i < sidedefs.size(); i++) {
      const bool is_selected = static_cast<size_t>(currentItem) == i;
      std::string optionLabel = std::to_string(i);

      if (ImGui::Selectable(optionLabel.c_str(), is_selected)) {
        currentItem = static_cast<int32_t>(i);
      }

      // Set the initial focus when opening the combo (scrolling to selection)
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
}

void EditorRenderer::createSelect(const char *label,
  const std::vector<std::uint16_t> &options,
  std::uint16_t currentItem,
  const std::function<void(uint16_t)> &setCurrElem,
  const std::function<void()> &addNewElem)
{
  if (ImGui::BeginCombo(label, fmt::format("Level: {}", currentItem).c_str())) {
    for (const std::uint16_t &option : options) {
      const bool is_selected = currentItem == option;
      if (ImGui::Selectable(fmt::format("Level {}", option).c_str(), is_selected)) {
        setCurrElem(option);
      }
      // Set the initial focus when opening the combo (scrolling to selection)
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    if (ImGui::Button("Add New")) {
      addNewElem();
    }
    ImGui::EndCombo();
  }
}


void EditorRenderer::drawVertex(const uint32_t vertexId, const float vertexRadius = g_defaultVertexRadius) const
{
  const auto &v = m_editor->state->findVertex(vertexId);
  if (v.x < 0 || v.y < 0 || m_sdlWindow.width <= v.x || m_sdlWindow.height <= v.y)
    return;

  ImDrawList *drawList = ImGui::GetWindowDrawList();

  ImU32 color;

  if (v.selected) {
    color = g_selectedColor;
  } else if (v.hovered) {
    color = g_hoverColor;
  } else {
    color = g_defaultColor;
  }

  drawList->AddCircleFilled(m_editor->state->transformedVertices[getObjectIndex(vertexId)], vertexRadius, color);
}
