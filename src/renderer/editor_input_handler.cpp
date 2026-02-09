#include "editor_input_handler.h"
#include "commands.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "math_utils.h"

#include <algorithm>
#include <ctime>
#include <iostream>
#include <limits>

namespace editor {

void EditorInputHandler::processInput(EditorState &state, commands::CommandHistory &history)
{
  static float_t s_vertexHoveringThresholdSq = 25.0f * 25.0f;
  static float_t s_lineHoveringThresholdSq = 25.0f * 25.0f;

  ImGuiIO &io = ImGui::GetIO();

  if (!io.WantCaptureKeyboard) {
    // reset the state when pressing escape
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { state.reset(); }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
      if (io.KeyShift) {
        history.redo(state);
      } else {
        history.undo(state);
      }
    }


    if (io.KeyCtrl && !io.WantCaptureMouse && io.MouseWheel != 0) {
      state.canvasZoom = std::clamp(state.canvasZoom * (io.MouseWheel > 0 ? 1.1f : 0.9f), 0.5f, 3.0f);
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

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left)) {
      if (state.isDragging) {
        state.draggingOffset = { mousePos.x - state.draggingStart.x, mousePos.y - state.draggingStart.y };
      } else {

        if (state.selection.empty()) {
          // start creating a line if clicking on empty space
          state.isCreatingLine = true;
          
        } else {
          // state dragging

          state.isDragging = true;
          state.draggingOffset = { 0, 0 };
          state.draggingStart = mousePos;
        }
      }

    } else {
      // flush and reset the dragging state when the mouse button is released
      flushDragging(state, history);

      state.isDragging = false;
      state.draggingOffset = { 0, 0 };
      state.draggingStart = { 0, 0 };
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

    float_t nodeDis = math_utils::getDistanceSq(vertex.toImVec2(), mousePos) - 4.0f;

    if (nodeDis < bestVertexDist) {
      bestVertexDist = nodeDis;
      bestVertexId = static_cast<uint32_t>(i);
    }
  }

  // finding the nearest lines which can be hovered/selected
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
        updateSelection(objectId, vertex.selected, state);
      }
    } else {
      auto &line = state.findLinedef(bestLineId);
      line.hovered = true;

      if (lmbClicked) {
        line.selected = !line.selected;
        auto objectId = makeObjectId(EditorObjectType::LINEDEF, bestLineId);
        updateSelection(objectId, line.selected, state);
      }
    }
  } else {
    // if clicking nothing, reset the selection
    if (lmbClicked) { resetSelection(state); }
  }
}

void EditorInputHandler::updateSelection(uint32_t id, bool selected, EditorState &state)
{
  ImGuiIO &io = ImGui::GetIO();

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
  resetSelection(state);

  if (selected) { state.selection.emplace_back(id); }
}

void EditorInputHandler::resetSelection(EditorState &state)
{
  for (uint32_t id : state.selection) {
    auto object = state.findObject(id);

    if (object) object->selected = false;
  }
  state.selection.clear();
}

void EditorInputHandler::flushDragging(editor::EditorState &state, commands::CommandHistory &history)
{
  for (uint32_t id : state.selection) {
    auto object = state.findObject(id);

    uint32_t index = editor::getObjectIndex(id);
    EditorObjectType type = editor::getObjectType(id);

    if (object) {
      switch (type) {
      case EditorObjectType::VERTEX: {
        auto &vertex = state.findVertex(index);
        auto cmd = std::make_unique<commands::MoveVertexCommand>(
          index, vertex, editor::EditorVertex(vertex.x + state.draggingOffset.x, vertex.y + state.draggingOffset.y));
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
