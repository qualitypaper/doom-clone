#include "editor_input_handler.h"
#include "commands.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "math_utils.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <ranges>
#include <unordered_map>

void EditorInputHandler::captureMouseDragging() const
{
  const ImVec2 mousePos = ImGui::GetMousePos();

  if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    if (const auto state = m_state.lock()) {
      if (state->selection.empty() && !state->isBlockSelecting) {
        state->isBlockSelecting = true;
        state->blockSelectionStart = mousePos;
        state->blockSelectionOffset = { 0, 0 };
      } else if (!state->isDragging && !state->isBlockSelecting) {
        state->isDragging = true;
        state->draggingOffset = { 0, 0 };
        state->draggingStart = mousePos;
      }
    }
  }
}

void EditorInputHandler::processMouseDragging() const
{
  const auto state = m_state.lock();
  if (!state) return;

  const ImGuiIO &io = ImGui::GetIO();
  const ImVec2 mousePos = io.MousePos;

  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsKeyReleased(ImGuiKey_LeftShift)) {
    flushDragging();

    state->isDragging = false;
    state->isShiftDragging = false;

    state->draggingOffset = { 0, 0 };
    state->draggingStart = { 0, 0 };
    state->shiftDraggingStart = { 0, 0 };
    state->shiftDraggingAxis = 0;
  } else if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
    if (!state->isShiftDragging) {
      state->isShiftDragging = true;
      state->shiftDraggingStart = mousePos;
      state->shiftDraggingAxis = 0;
    }

    if (state->shiftDraggingAxis == 0) {
      if (std::abs(io.MouseDelta.x) > std::abs(io.MouseDelta.y)) {
        state->shiftDraggingAxis = 1;
      } else if (std::abs(io.MouseDelta.y) > std::abs(io.MouseDelta.x)) {
        state->shiftDraggingAxis = 2;
      }
    }

    if (state->shiftDraggingAxis == 1) {
      state->draggingOffset.x = mousePos.x - state->shiftDraggingStart.x;
    } else if (state->shiftDraggingAxis == 2) {
      state->draggingOffset.y = mousePos.y - state->shiftDraggingStart.y;
    } else {
      state->draggingOffset = { mousePos.x - state->draggingStart.x, mousePos.y - state->draggingStart.y };
    }
  } else {
    state->draggingOffset = { mousePos.x - state->draggingStart.x, mousePos.y - state->draggingStart.y };
  }
}

void EditorInputHandler::processMouseRelatedInput() const
{
  const ImGuiIO &io = ImGui::GetIO();
  if (io.WantCaptureMouse) return;

  const auto state = m_state.lock();
  if (!state) return;

  const ImVec2 mousePos = ImGui::GetMousePos();

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    state->renderOptionsWindow = !state->renderOptionsWindow;
    state->optionsWindowPos = mousePos;
  }

  captureMouseDragging();

  if (state->isDragging) {
    processMouseDragging();
  } else if (state->isBlockSelecting) {
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      // flush block selecting
      flushBlockSelecting();

      state->isBlockSelecting = false;
      state->blockSelectionStart = { 0, 0 };
      state->blockSelectionOffset = { 0, 0 };
    } else {
      state->blockSelectionOffset = { mousePos.x - state->blockSelectionStart.x,
        mousePos.y - state->blockSelectionStart.y };
    }
  }

  // scrolling behavior
  if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Middle)) {
    if (state->isScrolling) {
      state->scrollingOffset = { state->scrollingOffset.x + io.MouseDelta.x,
        state->scrollingOffset.y + io.MouseDelta.y };
    } else {
      // state scrolling
      state->isScrolling = true;
      // state->scrollingOffset = { 0, 0 };
      if (state->scrollingStart.x == 0 && state->scrollingStart.y == 0) { state->scrollingStart = mousePos; }
    }
  } else if (state->isScrolling && ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
    state->isScrolling = false;
  }

  if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { resetSelection(); }
}

void EditorInputHandler::flushBlockSelecting() const
{
  const auto state = m_state.lock();
  if (!state) return;

  const AABB selectionBounds(
    { static_cast<int32_t>(state->blockSelectionStart.x), static_cast<int32_t>(state->blockSelectionStart.y) },
    { static_cast<int32_t>(state->blockSelectionStart.x + state->blockSelectionOffset.x),
      static_cast<int32_t>(state->blockSelectionStart.y + state->blockSelectionOffset.y) });


  for (size_t i = 0; i < state->transformedVertices.size(); i++) {
    const auto &v = state->transformedVertices[i];

    if (selectionBounds.contains(static_cast<int16_t>(v.x), static_cast<int16_t>(v.y))) {
      state->selection.emplace(makeObjectId(EditorObjectType::VERTEX, i));
      state->level->vertices[i].selected = true;
    }
  }

  for (size_t i = 0; i < state->level->linedefs.size(); i++) {
    auto &ld = state->level->linedefs[i];

    if (std::ranges::find(state->selection, makeObjectId(EditorObjectType::VERTEX, ld.end)) != state->selection.end()
        || std::ranges::find(state->selection, makeObjectId(EditorObjectType::VERTEX, ld.start))
             != state->selection.end()) {
      state->selection.emplace(makeObjectId(EditorObjectType::LINEDEF, i));
      ld.selected = true;
    }
  }
}

void EditorInputHandler::flushDragging() const
{
  const auto state = m_state.lock();
  if (!state) return;

  // early return if there is no dragging offset
  if (state->draggingOffset.x == 0 && state->draggingOffset.y == 0) return;

  // Collect encoded vertex object IDs that will be moved by selected linedefs
  std::unordered_map<uint32_t, bool> verticesMovedByLinedefs;
  verticesMovedByLinedefs.reserve(state->level->vertices.size());

  for (const uint32_t id : state->selection) {
    if (getObjectType(id) == EditorObjectType::LINEDEF) {
      const auto &ld = state->findLinedef(id);
      verticesMovedByLinedefs.emplace(ld.start, false);
      verticesMovedByLinedefs.emplace(ld.end, false);
    }
  }

  for (const uint32_t id : state->selection) {
    const auto object = state->findObject(id);
    if (!object) continue;

    const EditorObjectType type = getObjectType(id);

    switch (type) {
    case EditorObjectType::VERTEX: {
      if (verticesMovedByLinedefs.contains(id)) continue;

      ImVec2 unzoomedOffset{ state->draggingOffset.x / state->canvasZoom, state->draggingOffset.y / state->canvasZoom };

      auto cmd = std::make_unique<MoveVertexCommand>(id, unzoomedOffset);

      if (const auto history = m_commandHistory.lock()) { history->execute(std::move(cmd), *state); }
      break;
    }
    case EditorObjectType::LINEDEF: {
      const auto &line = state->findLinedef(id);
      auto &start = state->findVertex(line.start), &end = state->findVertex(line.end);

      std::unique_ptr<MoveLineDefCommand> cmd;
      ImVec2 unzoomedOffset{ state->draggingOffset.x / state->canvasZoom, state->draggingOffset.y / state->canvasZoom };

      if (!verticesMovedByLinedefs.at(line.start) && !verticesMovedByLinedefs.at(line.end)) {
        verticesMovedByLinedefs.at(line.start) = true;
        verticesMovedByLinedefs.at(line.end) = true;

        cmd = std::make_unique<MoveLineDefCommand>(
          id, start, end, start + unzoomedOffset, end + unzoomedOffset);
      } else if (verticesMovedByLinedefs.at(line.start) && !verticesMovedByLinedefs.at(line.end)) {
        verticesMovedByLinedefs.at(line.end) = true;

        cmd = std::make_unique<MoveLineDefCommand>(id, start, end, start, end + unzoomedOffset);
      } else if (!verticesMovedByLinedefs.at(line.start) && verticesMovedByLinedefs.at(line.end)) {
        verticesMovedByLinedefs.at(line.start) = true;

        cmd = std::make_unique<MoveLineDefCommand>(id, start, end, start + unzoomedOffset, end);
      } else {
        // skip if those vertices were already moved by other linedefs
        continue;
      }

      if (const auto history = m_commandHistory.lock()) { history->execute(std::move(cmd), *state); }
      break;
    }
    default:
      break;
    }
  }
}

void EditorInputHandler::processKeyboardInputs() const
{
  const ImGuiIO &io = ImGui::GetIO();

  if (io.WantCaptureKeyboard) return;

  const auto state = m_state.lock();
  if (!state) return;

  // reset the state when pressing escape
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { state->reset(); }

  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
    if (const auto history = m_commandHistory.lock()) {
      if (io.KeyShift) {
        history->redo(*state);
      } else {
        history->undo(*state);
      }
    }
  }

  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
    // save the current level into a .bin file
    // vertices, linedefs, sidedefs, sectors
    state->level->serialize("saved_level.bin");
  }

  if (io.KeyCtrl && !io.WantCaptureMouse && io.MouseWheel != 0) {
    state->canvasZoom = std::clamp(state->canvasZoom * (io.MouseWheel > 0 ? 1.1f : 0.9f), 0.5f, 3.0f);
  }
}

/**
 *
 * @param vertexRadius
 * @param hoveringThresholdSq
 * @return the nearest vertex/linedef to the cursor, if it passes a threshold
 * returns UINT32_MAX when none are in the range
 */
uint32_t EditorInputHandler::findNearestPastThreshold(const float vertexRadius,
  const float hoveringThresholdSq = 25.0f * 25.0f) const
{
  const auto state = m_state.lock();
  if (!state) return UINT32_MAX;

  const ImVec2 mousePos = ImGui::GetMousePos();

  uint32_t bestVertexIndex = 0;
  uint32_t bestLineIndex = 0;
  float_t bestVertexDist = FLT_MAX;
  float_t bestLineDist = FLT_MAX;

  for (size_t i = 0; i < state->level->vertices.size(); i++) {
    const ImVec2 vec = state->transformedVertices[i];

    const float_t nodeDis = math_utils::getDistanceSq(vec, mousePos) - vertexRadius * vertexRadius;

    if (nodeDis < bestVertexDist) {
      bestVertexDist = nodeDis;
      bestVertexIndex = static_cast<uint32_t>(i);
    }
  }

  // finding the nearest lines which can be hovered/selected
  for (size_t i = 0; i < state->level->linedefs.size(); i++) {
    const auto &ld = state->level->linedefs[i];

    const ImVec2 start = state->findTransformedVertex(ld.start);
    const ImVec2 end = state->findTransformedVertex(ld.end);

    const float_t distance = getDistanceToSegmentSq(start, end, mousePos);

    if (distance < bestLineDist) {
      bestLineDist = distance;
      bestLineIndex = static_cast<uint32_t>(i);
    }
  }

  if (bestLineDist < bestVertexDist && bestLineDist < hoveringThresholdSq) {
    return makeObjectId(EditorObjectType::LINEDEF, bestLineIndex);
  } else if (bestVertexDist < bestLineDist && bestVertexDist < hoveringThresholdSq) {
    return makeObjectId(EditorObjectType::VERTEX, bestVertexIndex);
  }

  return UINT32_MAX;
}

void EditorInputHandler::processNearestObject(const uint32_t bestObjectId) const
{
  if (bestObjectId == UINT32_MAX) return;

  const auto state = m_state.lock();
  if (!state) return;

  const bool lmbClicked = !ImGui::GetIO().WantCaptureMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

  const uint32_t bestObjectIndex = getObjectIndex(bestObjectId);

  switch (getObjectType(bestObjectId)) {
  case EditorObjectType::VERTEX: {
    // select vertex
    auto &vertex = state->level->vertices[bestObjectIndex];

    if (lmbClicked) {
      if (state->isCreatingLine) {
        // draw a line between the start vertex and the hovered vertex
        const EditorLineDef lineDef(state->lineStartVertexId, bestObjectId, LineDefType::REGULAR, -1, -1);

        auto cmd = std::make_unique<AddLineDefCommand>(lineDef);
        if (const auto history = m_commandHistory.lock()) { history->execute(std::move(cmd), *state); }

        state->isCreatingLine = false;
        state->lineStartVertexId = 0;
      } else {
        vertex.selected = !vertex.selected;

        updateSelection(bestObjectId, vertex.selected);
      }
    } else {
      vertex.hovered = true;
      state->hoveredObjectId = bestObjectId;
    }

    break;
  }
  case EditorObjectType::LINEDEF: {
    auto &line = state->level->linedefs[bestObjectIndex];

    if (lmbClicked) {
      line.selected = !line.selected;
      updateSelection(bestObjectId, line.selected);
    } else {
      line.hovered = true;
      state->hoveredObjectId = bestObjectId;
    }

    break;
  }
  default: {
  }
  }
}

void EditorInputHandler::processMouseInteractions(const float_t vertexRadius) const
{

  static float_t s_hoveringThresholdSq = 25.0f * 25.0f;

  const uint32_t bestObjectId = findNearestPastThreshold(vertexRadius, s_hoveringThresholdSq);

  processNearestObject(bestObjectId);
}

void EditorInputHandler::processInput(const float_t vertexRadius) const
{

  processKeyboardInputs();

  const ImVec2 mousePos = ImGui::GetMousePos();

  if (mousePos.x == -FLT_MAX || mousePos.y == -FLT_MAX) { return; }

  processMouseInteractions(vertexRadius);
  processMouseRelatedInput();
}


void EditorInputHandler::updateSelection(uint32_t id, const bool selected) const
{
  const auto state = m_state.lock();
  if (!state) return;

  const ImGuiIO &io = ImGui::GetIO();

  if (!io.WantCaptureKeyboard && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
    if (selected) {
      state->selection.emplace(id);
    } else {
      std::erase_if(state->selection, [id](auto &selectedId) { return selectedId == id; });
    }
  } else {
    resetSelection();

    if (selected) { state->selection.emplace(id); }
  }
}

void EditorInputHandler::resetSelection() const
{
  const auto state = m_state.lock();
  if (!state) return;

  for (const uint32_t id : state->selection) {
    EditorObject *object = state->findObject(id);

    if (object) object->selected = false;
  }
  state->selection.clear();
}


// @param origin is the base point from which the distance will be calculated
float_t EditorInputHandler::getDistanceToSegmentSq(const ImVec2 start, const ImVec2 end, const ImVec2 origin)
{
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
