#pragma once

#include "editor.h"

struct CommandHistory;

class Interaction
{
public:
  Interaction() = default;
  virtual ~Interaction() = default;

  virtual void hover() {}
  virtual void select() {}
};

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
  [[nodiscard]] std::pair<uint32_t, float> findNearestVertices(float vertexRadius) const;
  [[nodiscard]] std::pair<uint32_t, float> findNearestLinedefs() const;
  [[nodiscard]] uint32_t findNearestPastThreshold(float vertexRadius, bool shouldProcessLinedefs, float hoveringThresholdSq) const;
  void processNearestObject(uint32_t bestObjectId) const;
  void createLine(uint32_t toObjectId) const;
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
