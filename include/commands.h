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

struct MoveVertexCommand : Command
{
  uint32_t vertexId;
  editor::EditorVertex oldVertex, newVertex;

  MoveVertexCommand(const uint32_t _vertexId, editor::EditorVertex _oldVertex, editor::EditorVertex _newVertex)
    : vertexId(_vertexId), oldVertex(std::move(_oldVertex)), newVertex(std::move(_newVertex))
  {}

  void execute(editor::EditorState &state) override
  {
    auto &vertex = state.findVertex(vertexId);
    vertex.x = newVertex.x;
    vertex.y = newVertex.y;
  }

  void undo(editor::EditorState &state) override
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
    : lineDefId(_lineDefId), oldStart(std::move(_oldStart)), oldEnd(std::move(_oldEnd)), newStart(std::move(_newStart)),
      newEnd(std::move(_newEnd))
  {}

  void execute(editor::EditorState &state) override
  {
    const auto &lineDef = state.findLinedef(lineDefId);

    auto &startVertex = state.findVertex(editor::getObjectIndex(lineDef.start));
    auto &endVertex = state.findVertex(editor::getObjectIndex(lineDef.end));

    startVertex.x = newStart.x;
    startVertex.y = newStart.y;

    endVertex.x = newEnd.x;
    endVertex.y = newEnd.y;
  }

  void undo(editor::EditorState &state) override
  {
    const auto &lineDef = state.findLinedef(lineDefId);
    auto &startVertex = state.findVertex(editor::getObjectIndex(lineDef.start));
    auto &endVertex = state.findVertex(editor::getObjectIndex(lineDef.end));

    startVertex.x = oldStart.x;
    startVertex.y = oldStart.y;

    endVertex.x = oldEnd.x;
    endVertex.y = oldEnd.y;
  }
};

struct AddVertexCommand : Command
{
  editor::EditorVertex vertex;

  explicit AddVertexCommand(editor::EditorVertex _vertex) : vertex(std::move(_vertex)) {}

  void execute(editor::EditorState &state) override { state.level->vertices.emplace_back(vertex); }

  void undo(editor::EditorState &state) override { state.level->vertices.pop_back(); }
};

struct CommandHistory
{
  std::vector<std::unique_ptr<Command>> undoStack;
  std::vector<std::unique_ptr<Command>> redoStack;

  void execute(std::unique_ptr<Command> cmd, editor::EditorState &state)
  {
    cmd->execute(state);
    undoStack.emplace_back(std::move(cmd));
    // new action resets the redoStack
    redoStack.clear();
  }

  void undo(editor::EditorState &state)
  {
    if (undoStack.empty()) return;

    auto lastCmd = std::move(undoStack.back());
    lastCmd->undo(state);
    undoStack.pop_back();

    redoStack.emplace_back(std::move(lastCmd));
  }

  void redo(editor::EditorState &state)
  {
    if (redoStack.empty()) return;
    std::cout << "redoing command\n";

    auto undoneCommand = std::move(redoStack.back());
    redoStack.pop_back();

    undoneCommand->execute(state);
    undoStack.emplace_back(std::move(undoneCommand));
  }
};
}// namespace commands
