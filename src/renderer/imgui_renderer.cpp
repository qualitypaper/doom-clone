#include "imgui_renderer.h"
#include "math_utils.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace imguirenderer {

ImguiRenderer::ImguiRenderer(sdl_window::SdlWindow &sdlWindow) : sdlWindow(sdlWindow)
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
  SDL_RenderSetScale(sdlWindow.getRenderer(), currentIo.DisplayFramebufferScale.x, currentIo.DisplayFramebufferScale.y);

  SDL_RenderClear(sdlWindow.getRenderer());
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), sdlWindow.getRenderer());
  SDL_RenderPresent(sdlWindow.getRenderer());
}

void ImguiRenderer::render(gameloop::Level &level, const std::vector<ImVec4> &scaledLinedefs)
{
  static float_t lineHoveringThreshold = 25.0f;

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
  ImVec2 mousePos = ImGui::GetMousePos();

  int16_t hoverIndex = -1;
  static int16_t selectedIndex = -1;

  static std::unordered_map<float_t, int> distanceMap;
  static std::unordered_map<uint16_t, uint16_t> sectorCount;

  sectorCount.clear();
  distanceMap.clear();

  std::priority_queue<float_t, std::vector<float_t>, std::greater<float_t>> minHeap;

  for (uint16_t i = 0; i < scaledLinedefs.size(); i++) {
    ImVec2 start(scaledLinedefs[i].x, scaledLinedefs[i].y);
    ImVec2 end(scaledLinedefs[i].z, scaledLinedefs[i].w);

    if (mousePos.x < 0 || mousePos.y < 0 || mousePos.x > sdlWindow.width || mousePos.y > sdlWindow.height) { continue; }

    float_t distance = getDistanceToSegmentSq(start, end, mousePos);
    minHeap.push(distance);
    distanceMap.insert({ distance, i });
  }

  if (minHeap.size() > 0 && minHeap.top() < lineHoveringThreshold) { hoverIndex = distanceMap.at(minHeap.top()); }

  drawMapOutlines(scaledLinedefs, level, selectedIndex, hoverIndex);
  drawSelectedLinePopup(selectedIndex, level);

  ImGui::End();

  endFrame();
}

void ImguiRenderer::drawMapOutlines(const std::vector<ImVec4> &scaledLinedefs,
  gameloop::Level &level,
  int16_t &selectedIndex,
  int16_t hoverIndex)
{
  static ImU32 defaultColor = IM_COL32(255, 255, 255, 255);
  static ImU32 selectedColor = IM_COL32(0, 0, 255, 255);
  static ImU32 hoverColor = IM_COL32(255, 200, 0, 255);
  static float_t thickness = 2.0f;

  ImGuiIO &io = ImGui::GetIO();
  ImDrawList *drawList = ImGui::GetWindowDrawList();

  // draw outlines
  for (uint16_t i = 0; i < scaledLinedefs.size(); i++) {
    auto start = ImVec2(scaledLinedefs[i].x, scaledLinedefs[i].y);
    auto end = ImVec2(scaledLinedefs[i].z, scaledLinedefs[i].w);

    ImU32 color;
    bool lmbClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse;
    if (selectedIndex == i && lmbClicked) {
      // resetting the selection
      selectedIndex = -1;
      color = defaultColor;
    } else if (selectedIndex == i || (i == hoverIndex && lmbClicked)) {
      selectedIndex = i;
      // draw active line (blue)
      color = selectedColor;
    } else if (i == hoverIndex) {
      color = hoverColor;
    } else {
      // draw default line (white)
      color = defaultColor;

      if (level.linedefs[i].backSidedef != -1) {
        // tint a bit the color of the portal linedef
        color -= 0x32323200;
      }
    }
    drawList->AddLine(start, end, color, thickness);
  }
}

void ImguiRenderer::drawSelectedLinePopup(int16_t selectedIndex, gameloop::Level &level)
{
  if (selectedIndex == -1) return;

  auto &ld = level.linedefs[selectedIndex];
  gameloop::Vertex *start = &level.vertices[ld.start];
  gameloop::Vertex *end = &level.vertices[ld.end];

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

  createSelect("Front Sidedef: ", level.sidedefs, ld.frontSidedef);
  createSelect("Back Sidedef: ", level.sidedefs, ld.backSidedef);

  ImGui::End();
}

void ImguiRenderer::createSelect(const char *label,
  const std::vector<gameloop::SideDef> &sidedefs,
  int16_t &currentItem)
{
  if (ImGui::BeginCombo(label, std::to_string(currentItem).c_str())) {
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

// @param origin is the base point from which the distance will be calculated
float_t ImguiRenderer::getDistanceToSegmentSq(ImVec2 start, ImVec2 end, ImVec2 origin)
{
  ImVec2 startToOrigin(origin.x - start.x, origin.y - start.y);
  ImVec2 startToEnd(end.x - start.x, end.y - start.y);

  float_t startToEndLength = math_utils::getDistanceSq(start, end);

  // Via simple dot product rule: originToStart * cos(alpha) = <originToStart, startToEnd>/startToEndLength
  // which will be exactly the projection
  float_t projectedOriginToStartDistance = math_utils::dotProduct(startToOrigin, startToEnd) / startToEndLength;

  float_t clampedProjectionDistance = std::fmax(0, std::fmin(1, projectedOriginToStartDistance));

  // point which is orthogonal to the origin
  ImVec2 orthogonalPoint(
    start.x + startToEnd.x * clampedProjectionDistance, start.y + startToEnd.y * clampedProjectionDistance);

  // calculing distance from origin to the orthogonal point
  float_t dx = orthogonalPoint.x - origin.x;
  float_t dy = orthogonalPoint.y - origin.y;

  return dx * dx + dy * dy;
}


}// namespace imguirenderer
