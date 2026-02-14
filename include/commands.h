#pragma once
#include "editor.h"

#include <memory>
#include <utility>
#include <vector>

namespace commands {
struct Command
{
  virtual ~Command() = default;
  virtual void execute(editor::EditorState &state) = 0;
  virtual void undo(editor::EditorState &state) = 0;
};

struct AddLineDefCommand : Command
{
  editor::EditorLineDef lineDef;

  explicit AddLineDefCommand(editor::EditorLineDef _lineDef);

  void execute(editor::EditorState &state) override;
  void undo(editor::EditorState &state) override;
};

struct MoveVertexCommand : Command
{
  uint32_t vertexId;
  editor::EditorVertex oldVertex, newVertex;

  MoveVertexCommand(uint32_t _vertexId, editor::EditorVertex _oldVertex, editor::EditorVertex _newVertex);

  void execute(editor::EditorState &state) override;

  void undo(editor::EditorState &state) override;
};

struct MoveLineDefCommand : Command
{
  uint32_t lineDefId;
  editor::EditorVertex oldStart, oldEnd;
  editor::EditorVertex newStart, newEnd;

  MoveLineDefCommand(uint32_t _lineDefId,
    editor::EditorVertex _oldStart,
    editor::EditorVertex _oldEnd,
    editor::EditorVertex _newStart,
    editor::EditorVertex _newEnd);

  void execute(editor::EditorState &state) override;

  void undo(editor::EditorState &state) override;
};

struct AddVertexCommand : Command
{
  editor::EditorVertex vertex;

  explicit AddVertexCommand(editor::EditorVertex _vertex);

  void execute(editor::EditorState &state) override;

  void undo(editor::EditorState &state) override;
};

struct CommandHistory
{
  std::vector<std::unique_ptr<Command>> undoStack;
  std::vector<std::unique_ptr<Command>> redoStack;

  void execute(std::unique_ptr<Command> cmd, editor::EditorState &state);

  void undo(editor::EditorState &state);

  void redo(editor::EditorState &state);
};
}// namespace commands
