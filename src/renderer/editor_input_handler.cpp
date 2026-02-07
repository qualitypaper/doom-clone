#include "editor_input_handler.h"
#include "commands.h"
#include "imgui.h"
#include "math_utils.h"

#include <algorithm>
#include <iostream>
#include <limits>

namespace editor {

void EditorInputHandler::processInput(EditorState &state, commands::CommandHistory &history)
{
  static float_t s_vertexHoveringThresholdSq = 25.0f * 25.0f;
  static float_t s_lineHoveringThresholdSq = 25.0f * 25.0f;

  ImGuiIO &io = ImGui::GetIO();

  if (!io.WantCaptureKeyboard) {
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
      if (io.KeyShift) {
        history.redo(state);
      } else {
        history.undo(state);
      }
    }
  }

  ImVec2 mousePos = ImGui::GetMousePos();
  bool lmbClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse;

  if (mousePos.x < 0 || mousePos.x > state.width || mousePos.y < 0 || mousePos.y > state.height) { return; }

  if (!io.WantCaptureMouse) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      state.renderOptionsWindow = !state.renderOptionsWindow;
      state.optionsWindowPos = mousePos;
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      if (state.isDragging) {
        state.draggingOffset = { mousePos.x - state.draggingStart.x, mousePos.y - state.draggingStart.y };
      } else {
        state.isDragging = true;
        state.draggingOffset = { 0, 0 };
        state.draggingStart = mousePos;
      }
    } else {
      // flush the dragging state when the mouse button is released

      for (uint32_t id : state.selection) {
        auto object = state.findObject(id);

        uint32_t index = editor::getObjectIndex(id);
        EditorObjectType type = editor::getObjectType(id);

        if (object) {
          switch (type) {
          case EditorObjectType::VERTEX: {
            auto &vertex = state.findVertex(index);
            auto cmd = std::make_unique<commands::MoveVertexCommand>(index,
              vertex.x,
              vertex.y,
              vertex.x + static_cast<int16_t>(state.draggingOffset.x),
              vertex.y + static_cast<int16_t>(state.draggingOffset.y));
            history.execute(std::move(cmd), state);
            break;
          }
          case EditorObjectType::LINEDEF: {
            auto &line = state.findLinedef(index);
            auto &start = state.findVertex(line.start), &end = state.findVertex(line.end);
            auto cmd = std::make_unique<commands::MoveLineDefCommand>(
              index, start, end, start + state.draggingOffset, end + state.draggingOffset);
            history.execute(std::move(cmd), state);
            break;
          }
          default:
            break;
          }
        }

        state.isDragging = false;
        state.draggingOffset = { 0, 0 };
      }
    }
  }

  uint32_t bestVertexId = 0;
  uint32_t bestLineId = 0;
  float_t bestVertexDist = std::numeric_limits<float_t>::max();
  float_t bestLineDist = std::numeric_limits<float_t>::max();

  // fill the array of distances with distances to the vertices
  for (size_t i = 0; i < state.level->vertices.size(); i++) {
    auto &vertex = state.findVertex(i);
    // reset to the initial state
    vertex.hovered = false;

    float_t nodeDis = math_utils::getDistanceSq(vertex.toImVec2(), mousePos);

    if (nodeDis < bestVertexDist) {
      bestVertexDist = nodeDis;
      bestVertexId = static_cast<uint32_t>(i);
    }
  }

  // finding the nearest nodes/lines which can be hovered/selected
  for (size_t i = 0; i < state.level->linedefs.size(); i++) {
    auto &ld = state.findLinedef(i);
    // reset to the initial state
    ld.hovered = false;

    auto &start = state.findVertex(ld.start);
    auto &end = state.findVertex(ld.end);

    float_t distance = getDistanceToSegmentSq(start.toImVec2(), end.toImVec2(), mousePos);

    if (distance < bestLineDist) {
      bestLineDist = distance;
      bestLineId = static_cast<uint32_t>(i);
    }
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


  bool hasVertex = bestVertexDist < s_vertexHoveringThresholdSq;
  bool hasLine = bestLineDist < s_lineHoveringThresholdSq;

  if (hasVertex || hasLine) {
    bool chooseVertex = hasVertex && (!hasLine || bestVertexDist <= bestLineDist);

    if (chooseVertex) {
      auto &vertex = state.findVertex(bestVertexId);
      vertex.hovered = true;

      if (lmbClicked) {
        vertex.selected = !vertex.selected;
        auto objectId = makeObjectId(EditorObjectType::VERTEX, bestVertexId);
        updateSelection(objectId, vertex.selected);
      }
    } else {
      auto &line = state.findLinedef(bestLineId);
      line.hovered = true;

      if (lmbClicked) {
        line.selected = !line.selected;
        auto objectId = makeObjectId(EditorObjectType::LINEDEF, bestLineId);
        updateSelection(objectId, line.selected);
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
