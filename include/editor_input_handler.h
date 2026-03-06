#pragma once

#include "editor.h"

struct CommandHistory;

class EditorInputHandler
{
private:
  static void captureMouseDragging(EditorState &state);
  static void processMouseRelatedInput(EditorState &state, CommandHistory &history);
  static void processKeyboardInputs(EditorState &state, CommandHistory &history);
  static void updateSelection(uint32_t id, bool selected, EditorState &state);
  static void resetSelection(EditorState &state);

  static void flushBlockSelecting(EditorState &state);
  static void flushDragging(EditorState &state, CommandHistory &history);
  static void flushScrolling(EditorState &state, CommandHistory &history);

  static float_t getDistanceToSegmentSq(ImVec2 start, ImVec2 end, ImVec2 origin);

public:
  EditorInputHandler() = default;

public:
  static void processInput(EditorState &state, CommandHistory &history, float_t vertexRadius);
};
