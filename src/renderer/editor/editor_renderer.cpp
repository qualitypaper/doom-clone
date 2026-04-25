#include "editor_renderer.h"

#include "commands.h"
#include "core/math_utils.h"
#include "core/serialization/image_converter.h"
#include "core/serialization/texture_serializer.h"
#include "editor.h"
#include "magic_enum.h"

#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

static ImU32 g_defaultColor = IM_COL32(255, 255, 255, 255);
static ImU32 g_selectedColor = IM_COL32(0, 0, 255, 255);
static ImU32 g_hoverColor = IM_COL32(255, 200, 0, 255);

static float g_defaultVertexRadius = 4.0f;
static float g_defaultLinedefThickness = 2.0f;

EditorRenderer::EditorRenderer(std::shared_ptr<SdlWindow> sdlWindow, Editor &editor)
  : m_sdlWindow(std::move(sdlWindow)), m_editor(&editor)
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

  io.Fonts->AddFontDefaultVector();
  io.Fonts->AddFontDefaultBitmap();
  io.Fonts->AddFontFromFileTTF(PROJECT_ROOT_PATH "MartianMono.ttf");

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForSDLRenderer(m_sdlWindow->GetWindow(), m_sdlWindow->GetRenderer());
  ImGui_ImplSDLRenderer2_Init(m_sdlWindow->GetRenderer());
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
    m_sdlWindow->GetRenderer(), currentIo.DisplayFramebufferScale.x, currentIo.DisplayFramebufferScale.y);

  SDL_RenderClear(m_sdlWindow->GetRenderer());
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_sdlWindow->GetRenderer());
  SDL_RenderPresent(m_sdlWindow->GetRenderer());
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

void EditorRenderer::Render() const
{
  SDL_SetRenderDrawColor(m_sdlWindow->GetRenderer(), 0, 0, 0, 255);

  // Start the Dear ImGui frame
  startFrame();

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
  // draw FPS in the top right corner

  const std::string fpsText = std::format("FPS: {:.1f}", io.Framerate);
  const ImVec2 textSize = ImGui::CalcTextSize(fpsText.c_str());

  constexpr float kPadding = 10.0f;
  const float rightX = ImGui::GetWindowSize().x - textSize.x - kPadding;

  ImGui::SetCursorPos(ImVec2(rightX, kPadding));
  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", fpsText.c_str());

  // suggest creating a new line/vertex
  if (m_editor->state->renderOptionsWindow) {
    showVertexRLineCreation(m_editor->state->optionsWindowPos, m_editor->state->renderOptionsWindow);
  }

  drawCoordinatesCenter(vertexRadius);

  // render a window showing all the sectors
  drawSectorsWindow();

  // render a window showing all sidedefs
  drawSidedefsWindow();

  // render textures
  drawTexturesWindow();

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
  uint16_t currentLevelNum = m_editor->state->level->levelNum;

  createDropdown(
    "Select level",
    std::format("Map: {}", currentLevelNum),
    m_editor->state->numOfLevels,
    [&](const int optionIndex) {
      return optionIndex == currentLevelNum - 1;
    },
    [&](const int optionIndex) {
      return std::format("Map {}", optionIndex + 1);
    },
    [&](const int optionIndex) {
      currentLevelNum = optionIndex + 1;

      // level count begins with 1
      m_editor->ChangeLevel(currentLevelNum);
    },
    [this]() {
      if (ImGui::Button("Add New")) {
        m_editor->AddEmptyLevel();
      }
    });

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

void EditorRenderer::drawPropertiesTable(const char *tableId,
                                         const char *columnLabel,
                                         const std::function<void()> &drawContent) const
{
  constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH
    | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody
    | ImGuiTableFlags_ScrollY;// Allows the list to scroll if it exceeds window height

  if (!ImGui::BeginTable(tableId, 1, tableFlags)) {
    return;
  }

  ImGui::TableSetupColumn(columnLabel, ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableHeadersRow();

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);

  drawContent();

  ImGui::EndTable();
}

void EditorRenderer::drawSidedefsWindow() const
{
  ImGui::Begin("SideDefs");

  if (ImGui::Button("Create sidedef")) {
    // create new sidedef
    m_editor->state->level->sidedefs.emplace_back(-1, 0, 0, -1, -1, -1);
  }

  drawPropertiesTable("PropertyTable", "Sidedefs table", [this]() {
    // Flags for the parent category
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
        double xOffset = sd.xOffset;
        double yOffset = sd.yOffset;

        int lowerTextureId = sd.bottomWallTexture;
        int middleTextureId = sd.middleWallTexture;
        int upperTextureId = sd.upperWallTexture;

        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Sector id", &sectorId)) {
          // update sector id
          sd.sectorId = static_cast<int16_t>(sectorId);
        }

        ImGui::SetNextItemWidth(140);
        if (ImGui::InputInt(std::format("Lower texture id##sd{}_lower", i).c_str(), &lowerTextureId)) {
          sd.bottomWallTexture = static_cast<int16_t>(lowerTextureId);
        }
        ImGui::Text("Lower: %s", m_editor->state->texManager->GetFlatTextureName(sd.bottomWallTexture).c_str());

        ImGui::SetNextItemWidth(140);
        if (ImGui::InputInt(std::format("Middle texture id##sd{}_middle", i).c_str(), &middleTextureId)) {
          sd.middleWallTexture = static_cast<int16_t>(middleTextureId);
        }
        ImGui::Text("Middle: %s", m_editor->state->texManager->GetFlatTextureName(sd.middleWallTexture).c_str());

        ImGui::SetNextItemWidth(140);
        if (ImGui::InputInt(std::format("Upper texture id##sd{}_upper", i).c_str(), &upperTextureId)) {
          sd.upperWallTexture = static_cast<int16_t>(upperTextureId);
        }
        ImGui::Text("Upper: %s", m_editor->state->texManager->GetFlatTextureName(sd.upperWallTexture).c_str());

        ImGui::SetNextItemWidth(100);
        if (ImGui::InputDouble("Texture offset x", &xOffset)) {
          // update texture offset x
          sd.xOffset = xOffset;
        }
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputDouble("Texture offset y", &yOffset)) {
          // update texture offset y
          sd.yOffset = yOffset;
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
  });

  ImGui::End();
}

void EditorRenderer::drawSectorsWindow() const
{

  ImGui::Begin("Sectors");

  if (ImGui::Button("Create sector")) {
    m_editor->state->level->sectors.emplace_back();
  }

  drawPropertiesTable("PropertyTable", "Sectors table", [this]() {
    // Flags for the parent category
    constexpr ImGuiTreeNodeFlags categoryFlags = ImGuiTreeNodeFlags_SpanFullWidth;

    for (size_t i = 0; i < m_editor->state->level->sectors.size(); i++) {
      EditorSector &sector = m_editor->state->level->sectors[i];
      std::string label = "Sector " + std::to_string(i) + "##Sector";
      ImGui::PushID(i);
      const bool isTransformOpen = ImGui::TreeNodeEx(label.c_str(), categoryFlags);

      if (!isTransformOpen) {
        ImGui::PopID();
        continue;
      }

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

      int ceilingTex = sector.ceilingPic;
      int floorTex = sector.floorPic;

      ImGui::SetNextItemWidth(140);
      if (ImGui::InputInt(std::format("Ceil texture id##sd{}_upper", i).c_str(), &ceilingTex)) {
        sector.ceilingPic = static_cast<int16_t>(ceilingTex);
      }
      ImGui::Text("Ceiling: %s", m_editor->state->texManager->GetFlatTextureName(sector.ceilingPic).c_str());

      ImGui::SetNextItemWidth(140);
      if (ImGui::InputInt(std::format("Floor texture id##sd{}_upper", i).c_str(), &floorTex)) {
        sector.floorPic = static_cast<int16_t>(floorTex);
      }
      ImGui::Text("Floor: %s", m_editor->state->texManager->GetFlatTextureName(sector.floorPic).c_str());

      // ImGui::SetNextItemWidth(200);
      //
      // float color[3] = { ImGui::ColorConvertU32ToFloat4(sector.color).x,
      //                    ImGui::ColorConvertU32ToFloat4(sector.color).y,
      //                    ImGui::ColorConvertU32ToFloat4(sector.color).z };
      //
      // if (ImGui::ColorPicker3("Sector color", color)) {
      //   // update sector color
      //   sector.color = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], 1.0f));
      // }

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
      ImGui::PopID();

      // Pop the parent node
      ImGui::TreePop();
    }
  });

  ImGui::End();
}

void EditorRenderer::drawTexturesWindow() const
{
  ImGui::Begin("Textures");

  static std::map<size_t, std::array<char, 32>> s_flatTextureOriginPaths;

  drawPropertiesTable("PropertyTable", "Textures table", [this]() {
    TextureManager &textureManager = *m_editor->state->texManager;

    const auto renderTextureEntry = [&](Texture &texture,
                                        const size_t index,
                                        const char *groupId,
                                        const std::function<void(Texture &)> &renderExtra,
                                        const std::function<void(Texture &)> &renderPreview) {
      ImGui::PushID(groupId);
      ImGui::PushID(static_cast<int>(index));

      char *textureName = reinterpret_cast<char *>(texture.name.data());
      const bool opened = ImGui::TreeNodeEx(textureName, ImGuiTreeNodeFlags_SpanFullWidth);

      if (!opened) {
        ImGui::PopID();
        ImGui::PopID();
        return;
      }

      if (ImGui::InputText("Name", textureName, texture.name.size())) {
      }

      renderExtra(texture);
      renderPreview(texture);

      ImGui::TreePop();
      ImGui::PopID();
      ImGui::PopID();
    };

    if (ImGui::CollapsingHeader("Flat textures", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::Button("Add##flatTex")) {
        m_editor->state->texManager->AddDefaultFlatTexture();
      }

      for (size_t i = 0; i < textureManager.flatTextures.size(); ++i) {
        renderTextureEntry(
          textureManager.flatTextures[i],
          i,
          "flat",
          [&](Texture &baseTexture) {
            auto &texture = dynamic_cast<FlatTexture &>(baseTexture);

            // i is shifted by 32 bits to create a unique key for each texture
            // since both flat and wall textures can have the same index
            auto &originPath = s_flatTextureOriginPaths[i << 32];
            if (ImGui::InputText(std::format("Origin##flat{}", i).c_str(), originPath.data(), originPath.size())) {
            }

            if (ImGui::Button("Update") && originPath[0] != '\0') {
              if (!m_editor->state->palManager) {
                std::cerr << "Palette manager is not defined -> skip loading new image.\n";
                return;
              }

              m_editor->state->texManager->LoadFlatTexture(PROJECT_ROOT_PATH + std::string(originPath.data()), texture);
              texture.Write();

              std::cout << "Updated flat texture." << '\n';
              originPath.fill('\0');
            }
          },
          [this](Texture &texture) {
            if (texture.width <= 0 || texture.height <= 0 || !m_editor->state->palManager) {
              return;
            }

            m_editor->state->palManager->SetCurrentPalette(0);
            const auto &flatTexture = dynamic_cast<FlatTexture &>(texture);

            const std::vector<uint32_t> img = image::ConvertFlatToRaw(flatTexture.data, *m_editor->state->palManager);

            SDL_Texture *sdlTexture = nullptr;
            if (LoadTextureFromMemory(
                  img.data(), texture.width, texture.height, m_sdlWindow->GetRenderer(), &sdlTexture)) {
              ImGui::Image(sdlTexture, { static_cast<float>(texture.width), static_cast<float>(texture.height) });
            }
          });
      }
    }

    if (ImGui::CollapsingHeader("Patches", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::Button("Add##wallTex")) {
        m_editor->state->texManager->AddDefaultPatch();
      }

      for (size_t i = 0; i < textureManager.patches.size(); ++i) {
        renderTextureEntry(
          textureManager.patches[i],
          i,
          "wall",
          [&](Texture &baseTexture) {
            auto &texture = dynamic_cast<PatchView &>(baseTexture);

            auto &originPath = s_flatTextureOriginPaths[i << 32];
            if (ImGui::InputText(std::format("Origin##patch{}", i).c_str(), originPath.data(), originPath.size())) {
            }

            if (ImGui::Button("Update") && originPath[0] != '\0') {
              if (!m_editor->state->palManager) {
                std::cerr << "Palette manager is not defined -> skip loading new image.\n";
                return;
              }

              m_editor->state->texManager->LoadWallTexture(PROJECT_ROOT_PATH + std::string(originPath.data()), texture);
              texture.Write();

              std::cout << "Updated patch" << '\n';
              originPath.fill('\0');
            }
          },
          [this](Texture &texture) {
            if (texture.width <= 0 || texture.height <= 0 || !m_editor->state->palManager) {
              return;
            }

            m_editor->state->palManager->SetCurrentPalette(0);
            const auto &wallTexture = dynamic_cast<PatchView &>(texture);

            const std::vector<uint32_t> img = image::ConvertWallToRaw(static_cast<uint16_t>(texture.width),
                                                                       static_cast<uint16_t>(texture.height),
                                                                       wallTexture.data,
                                                                       *m_editor->state->palManager);

            SDL_Texture *sdlTexture = nullptr;
            if (LoadTextureFromMemory(
                  img.data(), texture.width, texture.height, m_sdlWindow->GetRenderer(), &sdlTexture)) {
              ImGui::Image(sdlTexture, { static_cast<float>(texture.width), static_cast<float>(texture.height) });
            }
          });
      }
    }


    if (ImGui::CollapsingHeader("Wall textures", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::Button("Add")) {
        m_editor->state->texManager->AddDefaultWallTexture();
      }


    }
  });

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
    math_utils::fromCenterCoordinates(EditorVertex(0, 0), m_sdlWindow->GetWidth(), m_sdlWindow->GetHeight());
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

  const std::string frontSideDefLabel = std::format(":Front SideDef ##LDSidedef{0}", ld.start);
  const std::string backSideDefLabel = std::format(":Back SideDef ##LDSidedef{0}", ld.end);

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

void EditorRenderer::createDropdown(const char *label,
                                    const std::string_view previewValue,
                                    const int itemCount,
                                    const std::function<bool(int)> &isSelected,
                                    const std::function<std::string(int)> &itemLabel,
                                    const std::function<void(int)> &onSelect,
                                    const std::function<void()> &drawFooter)
{
  if (!ImGui::BeginCombo(label, previewValue.data())) {
    return;
  }

  for (int i = 0; i < itemCount; i++) {
    const bool selected = isSelected(i);
    const std::string labelText = itemLabel(i);

    if (ImGui::Selectable(labelText.c_str(), selected)) {
      onSelect(i);
    }

    if (selected) {
      ImGui::SetItemDefaultFocus();
    }
  }

  if (drawFooter) {
    drawFooter();
  }

  ImGui::EndCombo();
}

void EditorRenderer::createSidedefSelect(const char *label,
                                         const std::vector<EditorSidedef> &sidedefs,
                                         int16_t &currentItem,
                                         const bool hasReset)
{
  const bool hasResetOption = hasReset || currentItem == -1;
  const int resetOptionOffset = hasResetOption ? 1 : 0;
  const int itemCount = static_cast<int>(sidedefs.size()) + resetOptionOffset;

  createDropdown(
    label,
    std::to_string(currentItem),
    itemCount,
    [currentItem, hasResetOption](const int optionIndex) {
      if (hasResetOption && optionIndex == 0) {
        return currentItem == -1;
      }

      if (currentItem < 0) {
        return false;
      }

      const int sidedefIndex = optionIndex - (hasResetOption ? 1 : 0);
      return static_cast<int>(currentItem) == sidedefIndex;
    },
    [hasResetOption](const int optionIndex) {
      if (hasResetOption && optionIndex == 0) {
        return std::string("-1");
      }

      return std::to_string(optionIndex - (hasResetOption ? 1 : 0));
    },
    [&currentItem, hasResetOption](const int optionIndex) {
      if (hasResetOption && optionIndex == 0) {
        currentItem = -1;
        return;
      }

      currentItem = static_cast<int16_t>(optionIndex - (hasResetOption ? 1 : 0));
    });
}

void EditorRenderer::createSelect(const char *label,
                                  const std::vector<std::uint16_t> &options,
                                  const uint16_t currentItem,
                                  const std::function<void(uint16_t)> &setCurrElem,
                                  const std::function<void()> &addNewElem)
{}


void EditorRenderer::drawVertex(const uint32_t vertexId, const float vertexRadius = g_defaultVertexRadius) const
{
  const auto &v = m_editor->state->findVertex(vertexId);
  if (v.x < 0 || v.y < 0 || m_sdlWindow->GetWidth() <= v.x || m_sdlWindow->GetHeight() <= v.y)
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
  if (!m_editor->state->transformedVertices.empty()
      && m_editor->state->transformedVertices.size() - 1 >= getObjectIndex(vertexId)) {
    drawList->AddCircleFilled(m_editor->state->transformedVertices[getObjectIndex(vertexId)], vertexRadius, color);
  }
}
