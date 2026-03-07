#pragma once

#include "editor.h"

struct CommandHistory;

class EditorInputHandler
{
private:
  std::weak_ptr<EditorState> m_state;
  std::weak_ptr<CommandHistory> m_commandHistory;

private:
  void captureMouseDragging() const;
  void processMouseDragging() const;
  void processMouseRelatedInput() const;
  void processKeyboardInputs() const;
  uint32_t findNearestPastThreshold(float vertexRadius, float hoveringThresholdSq) const;
  void processNearestObject(uint32_t bestObjectId) const;
  void processMouseInteractions(float_t vertexRadius) const;
  void updateSelection(uint32_t id, bool selected) const;
  void resetSelection() const;

  void flushBlockSelecting() const;
  void flushDragging() const;

  static float_t getDistanceToSegmentSq(ImVec2 start, ImVec2 end, ImVec2 origin);

public:
  EditorInputHandler() = default;
  EditorInputHandler(const std::shared_ptr<EditorState> &_state, const std::shared_ptr<CommandHistory> &_history)
    : m_state(_state), m_commandHistory(_history)
  {}

public:
  void processInput(float_t vertexRadius) const;
};
