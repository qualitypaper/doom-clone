#include "editor_input_handler.h"
#include "commands.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "math_utils.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace editor {

void EditorInputHandler::processMouseInputs(EditorState &state, commands::CommandHistory &history)
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
  }
}

void EditorInputHandler::processInput(EditorState &state, commands::CommandHistory &history, const float_t vertexRadius)
{
  static float_t s_vertexHoveringThresholdSq = 25.0f * 25.0f;
  static float_t s_lineHoveringThresholdSq = 25.0f * 25.0f;

  const ImGuiIO &io = ImGui::GetIO();

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

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
      // save the current level into a .bin file
      // vertices, linedefs, sidedefs, sectors

      std::ofstream file("saved_level.bin", std::ios::binary);

      std::cout << "Saving level to saved_level.bin...\n";
      std::cout << std::filesystem::current_path() << '\n';

      if (!file || !file.is_open() || file.fail()) {
        std::cerr << "Failed to open file for saving." << std::endl;
        return;
      }

      // write vertices
      uint64_t verticesCount = state.level->vertices.size();
      uint64_t linedefsCount = state.level->linedefs.size();
      uint64_t sidedefsCount = state.level->sidedefs.size();
      uint64_t sectorsCount = state.level->sectors.size();
      file.write(reinterpret_cast<char *>(&verticesCount), sizeof(verticesCount));
      for (auto &v : state.level->vertices) {
        file.write(reinterpret_cast<const char *>(&v.x), sizeof(v.x));
        file.write(reinterpret_cast<const char *>(&v.y), sizeof(v.y));
      }

      // write linedefs
      file.write(reinterpret_cast<char *>(&linedefsCount), sizeof(linedefsCount));
      for (auto &ld : state.level->linedefs) {
        const int16_t start = static_cast<int16_t>(ld.start);
        const int16_t end = static_cast<int16_t>(ld.end);
        const int16_t frontSidedef = static_cast<int16_t>(ld.frontSideDef);
        const int16_t backSidedef = static_cast<int16_t>(ld.backSideDef);
        file.write(reinterpret_cast<const char *>(&start), sizeof(start));
        file.write(reinterpret_cast<const char *>(&end), sizeof(end));
        file.write(reinterpret_cast<const char *>(&ld.type), sizeof(ld.type));
        file.write(reinterpret_cast<const char *>(&frontSidedef), sizeof(frontSidedef));
        file.write(reinterpret_cast<const char *>(&backSidedef), sizeof(backSidedef));
      }

      // write sidedefs
      file.write(reinterpret_cast<char *>(&sidedefsCount), sizeof(sidedefsCount));
      for (auto &sd : state.level->sidedefs) {
        file.write(reinterpret_cast<const char *>(&sd.sectorId), sizeof(sd.sectorId));
        file.write(reinterpret_cast<const char *>(&sd.xOffset), sizeof(sd.xOffset));
        file.write(reinterpret_cast<const char *>(&sd.yOffset), sizeof(sd.yOffset));
      }

      // write sectors
      file.write(reinterpret_cast<const char *>(&sectorsCount), sizeof(sectorsCount));
      for (auto &sec : state.level->sectors) {
        file.write(reinterpret_cast<const char *>(&sec.floorHeight), sizeof(sec.floorHeight));
        file.write(reinterpret_cast<const char *>(&sec.ceilingHeight), sizeof(sec.ceilingHeight));
        file.write(reinterpret_cast<const char *>(&sec.specialType), sizeof(sec.specialType));
        file.write(reinterpret_cast<const char *>(&sec.lightLevel), sizeof(sec.lightLevel));
        file.write(reinterpret_cast<const char *>(&sec.tag), sizeof(sec.tag));
      }

      file.close();
    }

    if (io.KeyCtrl && !io.WantCaptureMouse && io.MouseWheel != 0) {
      state.canvasZoom = std::clamp(state.canvasZoom * (io.MouseWheel > 0 ? 1.1f : 0.9f), 0.5f, 3.0f);
    }
  }

  const ImVec2 mousePos = ImGui::GetMousePos();
  const bool lmbClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse;

  if (mousePos.x < 0 || mousePos.x > static_cast<float_t>(state.width) || mousePos.y < 0
      || mousePos.y > static_cast<float_t>(state.height)) {
    return;
  }

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

    const float_t nodeDis = math_utils::getDistanceSq(vertex.toImVec2(), mousePos) - vertexRadius;

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
          const EditorLineDef lineDef(state.lineStartVertexId, bestVertexId, gameloop::LineDefType::REGULAR, -1, -1);

          auto cmd = std::make_unique<commands::AddLineDefCommand>(commands::AddLineDefCommand(lineDef));
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
    if (lmbClicked) { resetSelection(state); }
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
    auto object = state.findObject(id);

    if (object) object->selected = false;
  }
  state.selection.clear();
}

void EditorInputHandler::flushDragging(EditorState &state, commands::CommandHistory &history)
{
  // early return if there is no dragging offset
  if (state.draggingOffset.x == 0 && state.draggingOffset.y == 0) return;

  for (const uint32_t id : state.selection) {
    const auto object = state.findObject(id);

    uint32_t index = getObjectIndex(id);
    const EditorObjectType type = getObjectType(id);

    if (object) {
      switch (type) {
      case EditorObjectType::VERTEX: {
        auto &vertex = state.findVertex(index);
        auto cmd = std::make_unique<commands::MoveVertexCommand>(index,
          vertex,
          EditorVertex(static_cast<int32_t>(static_cast<float_t>(vertex.x) + state.draggingOffset.x),
            static_cast<int32_t>(static_cast<float_t>(vertex.y) + state.draggingOffset.y)));

        history.execute(std::move(cmd), state);
        break;
      }
      case EditorObjectType::LINEDEF: {
        const auto &line = state.findLinedef(index);
        auto &start = state.findVertex(line.start), &end = state.findVertex(line.end);

        auto cmd = std::make_unique<commands::MoveLineDefCommand>(
          index, start, end, start.add(state.draggingOffset), end.add(state.draggingOffset));

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
float_t EditorInputHandler::getDistanceToSegmentSq(const ImVec2 start, const ImVec2 end, const ImVec2 origin)
{
  assert(start.x != end.x || start.y != end.y);

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
}// namespace editor
