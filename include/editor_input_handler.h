#pragma once

#include "commands.h"
#include "editor.h"

namespace editor {
struct EditorInputHandler
{
  EditorInputHandler() = default;

  void processInput(EditorState &state, commands::CommandHistory &history);
  void resetSelection(EditorState &state);
  void flushDragging(editor::EditorState &state, commands::CommandHistory &history);
  void updateSelection(uint32_t id, bool selected, EditorState &state);
  
  float_t getDistanceToSegmentSq(ImVec2 start, ImVec2 end, ImVec2 origin);
};
}// namespace editor
