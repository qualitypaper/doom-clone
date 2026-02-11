#pragma once

#include "commands.h"
#include "editor.h"

namespace editor {
struct EditorInputHandler
{
  EditorInputHandler() = default;

  static void processInput(EditorState &state, commands::CommandHistory &history, float_t vertexRadius);
  static void updateSelection(uint32_t id, bool selected, EditorState &state);
  static void resetSelection(EditorState &state);
  static void flushDragging(EditorState &state, commands::CommandHistory &history);
  static float_t getDistanceToSegmentSq(ImVec2 start, ImVec2 end, ImVec2 origin);
};
}// namespace editor
