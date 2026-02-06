#pragma once
#include "editor.h"

#include <memory>
#include <vector>

namespace commands {
struct Command
{
  virtual ~Command() = default;
  virtual void execute(editor::EditorState &state) = 0;
  virtual void undo(editor::EditorState &state) = 0;
};

struct AddVertexCommand : Command
{
  editor::EditorVertex vertex;

  AddVertexCommand(editor::EditorVertex _vertex) : vertex(_vertex) {}

  void execute(editor::EditorState &state) { state.level->vertices.emplace_back(vertex); }

  void undo(editor::EditorState &state) { state.level->vertices.pop_back(); }
};

struct CommandHistory
{
  std::vector<std::unique_ptr<Command>> undoStack;
  std::vector<std::unique_ptr<Command>> redoStack;

  void execute(std::unique_ptr<Command> cmd, editor::EditorState &state)
  {
    cmd.get()->execute(state);
    undoStack.emplace_back(std::move(cmd));
    // new action resets the redoStack
    redoStack.clear();
  }

  void undo(editor::EditorState &state)
  {
    auto &lastCmd = undoStack.back();
    lastCmd.get()->undo(state);

    undoStack.pop_back();
  }

  void redo(editor::EditorState &state)
  {
    auto &undoneCommand = redoStack.back();
    undoneCommand.get()->execute(state);
    redoStack.pop_back();
  }
};
}// namespace commands
