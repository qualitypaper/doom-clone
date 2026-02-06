#include "editor_input_handler.h"
#include "commands.h"
#include "imgui.h"
#include "math_utils.h"

#include <algorithm>
#include <queue>
#include <unordered_map>
#include <iostream>

namespace editor {

void EditorInputHandler::processInput(EditorState &state, commands::CommandHistory &history)
{
  static float_t s_lineHoveringThreshold = 25.0f;

  ImGuiIO &io = ImGui::GetIO();

  ImVec2 mousePos = ImGui::GetMousePos();
  bool lmbClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse;

  if (!io.WantCaptureKeyboard) {
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
      if (io.KeyShift) {
        history.redo(state);
      } else {
        history.undo(state);
      }
    }
  }

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    state.renderOptionsWindow = !state.renderOptionsWindow;
    state.optionsWindowPos = mousePos;
  }

  static std::unordered_map<float_t, uint32_t> s_distanceMap;
  static std::unordered_map<uint16_t, uint16_t> s_sectorCount;
  s_distanceMap.reserve(state.level->linedefs.size());

  s_sectorCount.clear();
  s_distanceMap.clear();

  std::priority_queue<float_t, std::vector<float_t>, std::greater<float_t>> minHeap;

  // fill the array of distances with distances to the vertices
  for (auto &vertex : state.level->vertices) {
    // reset to the initial state
    vertex.hovered = false;

    float_t nodeDis = math_utils::getDistanceSq(ImVec2(vertex.x, vertex.y), mousePos);

    minHeap.emplace(nodeDis);
    s_distanceMap.emplace(nodeDis, vertex.id);
  }

  // finding the nearest nodes/lines which can be hovered/selected
  for (auto &ld : state.level->linedefs) {
    // reset to the initial state
    ld.hovered = false;

    auto start = state.findVertex(ld.start);
    auto end = state.findVertex(ld.end);

    if (!start || !end) continue;

    float_t distance = getDistanceToSegmentSq(start->toImVec2(), end->toImVec2(), mousePos);

    minHeap.emplace(distance);

    s_distanceMap.emplace(distance, ld.id);
  }

  static auto updateSelection = [&](uint32_t id, bool selected) {
    if (io.KeyCtrl && !io.WantCaptureKeyboard) {
      if (selected) {
        state.selection.emplace_back(id);
      } else {
        state.selection.erase(
          std::remove_if(
            state.selection.begin(), state.selection.end(), [id](auto &selectedId) { return selectedId == id; }),
          state.selection.end());
      }
      return;
    }

    for (uint32_t id : state.selection) {
      auto object = state.findObject(id);

      if (object) object->selected = false;
    }
    state.selection.clear();
    std::cout << "Selection cleared: " << '\n';
    if (selected) { state.selection.emplace_back(id); }
  };

  if (minHeap.size() > 0 && minHeap.top() < s_lineHoveringThreshold) {
    auto id = s_distanceMap.at(minHeap.top());

    if (auto vertex = state.findVertex(id)) {
      vertex->hovered = true;

      if (lmbClicked) {
        vertex->selected = !vertex->selected;
        updateSelection(id, vertex->selected);
      }
    } else if (auto line = state.findLinedef(id)) {
      line->hovered = true;

      if (lmbClicked) {
        line->selected = !line->selected;
        updateSelection(id, line->selected);
      }
    }
  }
}

// @param origin is the base point from which the distance will be calculated
float_t EditorInputHandler::getDistanceToSegmentSq(ImVec2 start, ImVec2 end, ImVec2 origin)
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
}// namespace editor
