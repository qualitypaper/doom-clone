#include "editor_input_handler.h"
#include "commands.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "math_utils.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <set>
#include <unordered_map>

void EditorInputHandler::processMouseInputs(EditorState &state, CommandHistory &history)
{
  const ImGuiIO &io = ImGui::GetIO();
  if (io.WantCaptureMouse) return;

  const ImVec2 mousePos = ImGui::GetMousePos();

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    state.renderOptionsWindow = !state.renderOptionsWindow;
    state.optionsWindowPos = mousePos;
  }

  if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left)) {
    if (state.isDragging) {
      state.draggingOffset = { mousePos.x - state.draggingStart.x, mousePos.y - state.draggingStart.y };
    } else if (state.isBlockSelecting) {
      state.blockSelectionOffset = { mousePos.x - state.blockSelectionStart.x,
        mousePos.y - state.blockSelectionStart.y };
    } else {
      if (state.selection.empty()) {
        // start block selection
        state.isBlockSelecting = true;
        state.blockSelectionStart = mousePos;
      } else {
        // state dragging
        state.isDragging = true;
        state.draggingOffset = { 0, 0 };
        state.draggingStart = mousePos;
      }
    }

  } else if (state.isDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    // flush and reset the dragging state when the mouse button is released
    flushDragging(state, history);

    state.isDragging = false;
    state.draggingOffset = { 0, 0 };
    state.draggingStart = { 0, 0 };
  } else if (state.isBlockSelecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    // flush block selecting
    flushBlockSelecting(state);

    state.isBlockSelecting = false;
    state.blockSelectionStart = { 0, 0 };
    state.blockSelectionOffset = { 0, 0 };
  }
}

void EditorInputHandler::flushBlockSelecting(EditorState &state)
{
  const AABB selectionBounds(
    { static_cast<int32_t>(state.blockSelectionStart.x), static_cast<int32_t>(state.blockSelectionStart.y) },
    { static_cast<int32_t>(state.blockSelectionStart.x + state.blockSelectionOffset.x),
      static_cast<int32_t>(state.blockSelectionStart.y + state.blockSelectionOffset.y) });


  for (size_t i = 0; i < state.level->vertices.size(); i++) {
    auto &v = state.level->vertices[i];

    if (selectionBounds.contains(static_cast<int16_t>(v.x), static_cast<int16_t>(v.y))) {
      state.selection.emplace_back(makeObjectId(EditorObjectType::VERTEX, i));
      v.selected = true;
    }
  }

  for (size_t i = 0; i < state.level->linedefs.size(); i++) {
    auto &ld = state.level->linedefs[i];

    if (std::ranges::find(state.selection, makeObjectId(EditorObjectType::VERTEX, ld.end)) != state.selection.end()
        || std::ranges::find(state.selection, makeObjectId(EditorObjectType::VERTEX, ld.start))
             != state.selection.end()) {
      state.selection.emplace_back(makeObjectId(EditorObjectType::LINEDEF, i));
      ld.selected = true;
    }
  }
}

void EditorInputHandler::processKeyboardInputs(EditorState &state, CommandHistory &history)
{
  const ImGuiIO &io = ImGui::GetIO();

  if (io.WantCaptureKeyboard) return;

  // reset the state when pressing escape
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { state.reset(); }

  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
    if (io.KeyShift) {
      history.redo(state);
    } else {
      history.undo(state);
    }
  }

  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
    // save the current level into a .bin file
    // vertices, linedefs, sidedefs, sectors
    state.level->serialize("saved_level.bin");
  }

  if (io.KeyCtrl && !io.WantCaptureMouse && io.MouseWheel != 0) {
    state.canvasZoom = std::clamp(state.canvasZoom * (io.MouseWheel > 0 ? 1.1f : 0.9f), 0.5f, 3.0f);
  }
}

void EditorInputHandler::processInput(EditorState &state, CommandHistory &history, const float_t vertexRadius)
{
  static float_t s_vertexHoveringThresholdSq = 25.0f * 25.0f;
  static float_t s_lineHoveringThresholdSq = 25.0f * 25.0f;

  processKeyboardInputs(state, history);

  const ImVec2 mousePos = ImGui::GetMousePos();

  if (mousePos.x < 0 || mousePos.x > static_cast<float_t>(state.width) || mousePos.y < 0
      || mousePos.y > static_cast<float_t>(state.height)) {
    return;
  }

  const ImGuiIO &io = ImGui::GetIO();
  const bool lmbClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse;

  processMouseInputs(state, history);

  uint32_t bestVertexId = 0;
  uint32_t bestLineId = 0;
  float_t bestVertexDist = std::numeric_limits<float_t>::max();
  float_t bestLineDist = std::numeric_limits<float_t>::max();

  // fill the array of distances with distances to the vertices
  for (size_t i = 0; i < state.level->vertices.size(); i++) {
    auto &vertex = state.findVertex(i);
    // reset to the initial state
    vertex.hovered = false;

    const float_t nodeDis = math_utils::getDistanceSq(vertex.toImVec2(), mousePos) - vertexRadius * vertexRadius;

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

    const float_t distance = getDistanceToSegmentSq(start.toImVec2(), end.toImVec2(), mousePos);

    if (distance < bestLineDist) {
      bestLineDist = distance;
      bestLineId = static_cast<uint32_t>(i);
    }
  }

  const bool hasVertex = bestVertexDist < s_vertexHoveringThresholdSq;
  const bool hasLine = bestLineDist < s_lineHoveringThresholdSq;

  if (hasVertex || hasLine) {
    if (hasVertex && (!hasLine || bestVertexDist <= bestLineDist)) {
      auto &vertex = state.findVertex(bestVertexId);

      if (lmbClicked) {
        if (state.isCreatingLine) {
          // draw a line between the start vertex and the hovered vertex
          const EditorLineDef lineDef(state.lineStartVertexId, bestVertexId, LineDefType::REGULAR, -1, -1);

          auto cmd = std::make_unique<AddLineDefCommand>(lineDef);
          history.execute(std::move(cmd), state);
          state.isCreatingLine = false;
          state.lineStartVertexId = 0;
        } else {
          vertex.selected = !vertex.selected;
          const uint32_t objectId = makeObjectId(EditorObjectType::VERTEX, bestVertexId);
          updateSelection(objectId, vertex.selected, state);
        }
      } else {
        vertex.hovered = true;
      }
    } else {
      auto &line = state.findLinedef(bestLineId);

      if (lmbClicked) {
        line.selected = !line.selected;
        const uint32_t objectId = makeObjectId(EditorObjectType::LINEDEF, bestLineId);
        updateSelection(objectId, line.selected, state);
      } else {
        line.hovered = true;
      }
    }
  } else {
    // if clicking nothing, reset the selection
    // if (lmbClicked) { resetSelection(state); }
  }
}


void EditorInputHandler::updateSelection(uint32_t id, const bool selected, EditorState &state)
{
  const ImGuiIO &io = ImGui::GetIO();

  if (io.KeyCtrl && !io.WantCaptureKeyboard) {
    if (selected) {
      state.selection.emplace_back(id);
    } else {
      std::erase_if(state.selection, [id](auto &selectedId) { return selectedId == id; });
    }
    return;
  }
  resetSelection(state);

  if (selected) { state.selection.emplace_back(id); }
}

void EditorInputHandler::resetSelection(EditorState &state)
{
  for (const uint32_t id : state.selection) {
    EditorObject *object = state.findObject(id);

    if (object) object->selected = false;
  }
  state.selection.clear();
}

void EditorInputHandler::flushDragging(EditorState &state, CommandHistory &history)
{
  // early return if there is no dragging offset
  if (state.draggingOffset.x == 0 && state.draggingOffset.y == 0) return;

  // Collect encoded vertex object IDs that will be moved by selected linedefs
  std::unordered_map<uint32_t, bool> verticesMovedByLinedefs;

  for (const uint32_t id : state.selection) {
    if (getObjectType(id) == EditorObjectType::LINEDEF) {
      const auto &ld = state.findLinedef(id);
      verticesMovedByLinedefs.emplace(ld.start, false);
      verticesMovedByLinedefs.emplace(ld.end, false);
    }
  }

  for (const uint32_t id : state.selection) {
    const auto object = state.findObject(id);
    if (!object) continue;

    const EditorObjectType type = getObjectType(id);

    switch (type) {
    case EditorObjectType::VERTEX: {
      if (verticesMovedByLinedefs.contains(id)) continue;

      auto &vertex = state.findVertex(id);
      auto cmd = std::make_unique<MoveVertexCommand>(id,
        vertex,
        EditorVertex(static_cast<int32_t>(static_cast<float_t>(vertex.x) + state.draggingOffset.x),
          static_cast<int32_t>(static_cast<float_t>(vertex.y) + state.draggingOffset.y)));

      history.execute(std::move(cmd), state);
      break;
    }
    case EditorObjectType::LINEDEF: {
      const auto &line = state.findLinedef(id);
      auto &start = state.findVertex(line.start), &end = state.findVertex(line.end);

      std::unique_ptr<MoveLineDefCommand> cmd;

      if (!verticesMovedByLinedefs.at(line.start) && !verticesMovedByLinedefs.at(line.end)) {
        verticesMovedByLinedefs.at(line.start) = true;
        verticesMovedByLinedefs.at(line.end) = true;

        cmd = std::make_unique<MoveLineDefCommand>(
          id, start, end, start + state.draggingOffset, end + state.draggingOffset);
      } else if (verticesMovedByLinedefs.at(line.start) && !verticesMovedByLinedefs.at(line.end)) {
        verticesMovedByLinedefs.at(line.end) = true;

        cmd = std::make_unique<MoveLineDefCommand>(id, start, end, start, end + state.draggingOffset);
      } else if (!verticesMovedByLinedefs.at(line.start) && verticesMovedByLinedefs.at(line.end)) {
        verticesMovedByLinedefs.at(line.start) = true;

        cmd = std::make_unique<MoveLineDefCommand>(id, start, end, start + state.draggingOffset, end);
      } else {
        // skip if those vertices were already moved by other linedefs
        continue;
      }

      history.execute(std::move(cmd), state);
      break;
    }
    default:
      break;
    }
  }
}

// @param origin is the base point from which the distance will be calculated
float_t EditorInputHandler::getDistanceToSegmentSq(const ImVec2 start, const ImVec2 end, const ImVec2 origin)
{
  // assert(!(start.x == end.x && start.y == end.y));

  const ImVec2 startToOrigin(origin.x - start.x, origin.y - start.y);
  const ImVec2 startToEnd(end.x - start.x, end.y - start.y);

  const float_t startToEndLength = math_utils::getDistanceSq(start, end);

  // Via simple dot product rule: originToStart * cos(alpha) = <originToStart, startToEnd>/startToEndLength
  // which will be exactly the projection
  const float_t projectedOriginToStartDistance = math_utils::dotProduct(startToOrigin, startToEnd) / startToEndLength;

  const float_t clampedProjectionDistance = std::clamp(projectedOriginToStartDistance, 0.0f, 1.0f);

  // point which is orthogonal to the origin
  const ImVec2 orthogonalPoint(
    start.x + startToEnd.x * clampedProjectionDistance, start.y + startToEnd.y * clampedProjectionDistance);

  // calculing distance from origin to the orthogonal point
  const float_t dx = orthogonalPoint.x - origin.x;
  const float_t dy = orthogonalPoint.y - origin.y;

  return dx * dx + dy * dy;
}
