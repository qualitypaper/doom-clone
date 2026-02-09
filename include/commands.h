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
  editor::EditorVertex oldVertex, newVertex;

  MoveVertexCommand(uint32_t _vertexId, editor::EditorVertex _oldVertex, editor::EditorVertex _newVertex)
    : vertexId(_vertexId), oldVertex(_oldVertex), newVertex(_newVertex)
  {}

  void execute(editor::EditorState &state)
  {
    auto &vertex = state.findVertex(vertexId);
    vertex.x = newVertex.x;
    vertex.y = newVertex.y;
  }

  void undo(editor::EditorState &state)
  {
    auto &vertex = state.findVertex(vertexId);
    vertex.x = oldVertex.x;
    vertex.y = oldVertex.y;
  }
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
    editor::EditorVertex _newEnd)
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
