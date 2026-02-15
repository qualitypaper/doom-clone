#pragma once
#include "editor.h"

#include <memory>
#include <utility>
#include <vector>

struct Command
{
  virtual ~Command() = default;
  virtual void execute(EditorState &state) = 0;
  virtual void undo(EditorState &state) = 0;
};

struct AddLineDefCommand : Command
{
  EditorLineDef lineDef;

  explicit AddLineDefCommand(EditorLineDef _lineDef);

  void execute(EditorState &state) override;
  void undo(EditorState &state) override;
};

struct MoveVertexCommand : Command
{
  uint32_t vertexId;
  EditorVertex oldVertex, newVertex;

  MoveVertexCommand(uint32_t _vertexId, EditorVertex _oldVertex, EditorVertex _newVertex);

  void execute(EditorState &state) override;

  void undo(EditorState &state) override;
};

struct MoveLineDefCommand : Command
{
  uint32_t lineDefId;
  EditorVertex oldStart, oldEnd;
  EditorVertex newStart, newEnd;

  MoveLineDefCommand(uint32_t _lineDefId,
    EditorVertex _oldStart,
    EditorVertex _oldEnd,
    EditorVertex _newStart,
    EditorVertex _newEnd);

  void execute(EditorState &state) override;

  void undo(EditorState &state) override;
};

struct AddVertexCommand : Command
{
  EditorVertex vertex;

  explicit AddVertexCommand(EditorVertex _vertex);

  void execute(EditorState &state) override;

  void undo(EditorState &state) override;
};

struct CommandHistory
{
  std::vector<std::unique_ptr<Command>> undoStack;
  std::vector<std::unique_ptr<Command>> redoStack;

  void execute(std::unique_ptr<Command> cmd, EditorState &state);

  void undo(EditorState &state);

  void redo(EditorState &state);
};
