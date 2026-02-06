#pragma once

#include "commands.h"
#include "editor.h"

namespace editor {
struct EditorInputHandler
{
  EditorInputHandler() = default;

  void processInput(EditorState &state, commands::CommandHistory &history);
  float_t getDistanceToSegmentSq(ImVec2 start, ImVec2 end, ImVec2 origin);
};
}// namespace editor
