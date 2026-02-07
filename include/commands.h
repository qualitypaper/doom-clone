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

struct MoveVertexCommand : Command
{
  uint32_t vertexId;
  int16_t oldX, oldY;
  int16_t newX, newY;

  MoveVertexCommand(uint32_t _vertexId, int16_t _oldX, int16_t _oldY, int16_t _newX, int16_t _newY)
    : vertexId(_vertexId), oldX(_oldX), oldY(_oldY), newX(_newX), newY(_newY)
  {}

  void execute(editor::EditorState &state)
  {
    auto &vertex = state.findVertex(vertexId);
    vertex.x = newX;
    vertex.y = newY;
  }

  void undo(editor::EditorState &state)
  {
    auto &vertex = state.findVertex(vertexId);
    vertex.x = oldX;
    vertex.y = oldY;
  }
};

struct MoveLineDefCommand : Command {
  uint32_t lineDefId;
  editor::EditorVertex oldStart, oldEnd;
  editor::EditorVertex newStart, newEnd;

  MoveLineDefCommand(uint32_t _lineDefId, editor::EditorVertex _oldStart, editor::EditorVertex _oldEnd,
    editor::EditorVertex _newStart, editor::EditorVertex _newEnd)
    : lineDefId(_lineDefId), oldStart(_oldStart), oldEnd(_oldEnd), newStart(_newStart), newEnd(_newEnd)
  {}

  void execute(editor::EditorState &state)
  {
    auto &lineDef = state.findLinedef(lineDefId);
    state.findVertex(editor::getObjectIndex(lineDef.start)) = newStart;
    state.findVertex(editor::getObjectIndex(lineDef.end)) = newEnd;
  }

  void undo(editor::EditorState &state)
  {
    auto &lineDef = state.findLinedef(lineDefId);
    state.findVertex(editor::getObjectIndex(lineDef.start)) = oldStart;
    state.findVertex(editor::getObjectIndex(lineDef.end)) = oldEnd;
  }
};

struct AddVertexCommand : Command
{
  editor::EditorVertex vertex;

  AddVertexCommand(editor::EditorVertex _vertex) : vertex(_vertex) {}

  void execute(editor::EditorState &state)
  {
    state.level->vertices.emplace_back(vertex);
  }

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
