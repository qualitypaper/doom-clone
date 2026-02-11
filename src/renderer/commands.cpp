#include "commands.h"

namespace commands {
AddLineDefCommand::AddLineDefCommand(editor::EditorLineDef _lineDef) : lineDef(std::move(_lineDef)) {}

void AddLineDefCommand::execute(editor::EditorState &state)
{
  state.level->linedefs.emplace_back(lineDef);
}

void AddLineDefCommand::undo(editor::EditorState &state)
{
  state.level->linedefs.pop_back();
}

MoveVertexCommand::MoveVertexCommand(const uint32_t _vertexId,
  editor::EditorVertex _oldVertex,
  editor::EditorVertex _newVertex)
  : vertexId(_vertexId), oldVertex(std::move(_oldVertex)), newVertex(std::move(_newVertex))
{}

void MoveVertexCommand::execute(editor::EditorState &state)
{
  auto &vertex = state.findVertex(vertexId);
  vertex.x = newVertex.x;
  vertex.y = newVertex.y;
}

void MoveVertexCommand::undo(editor::EditorState &state)
{
  auto &vertex = state.findVertex(vertexId);
  vertex.x = oldVertex.x;
  vertex.y = oldVertex.y;
}

MoveLineDefCommand::MoveLineDefCommand(uint32_t _lineDefId,
  editor::EditorVertex _oldStart,
  editor::EditorVertex _oldEnd,
  editor::EditorVertex _newStart,
  editor::EditorVertex _newEnd)
  : lineDefId(_lineDefId), oldStart(std::move(_oldStart)), oldEnd(std::move(_oldEnd)),
    newStart(std::move(_newStart)), newEnd(std::move(_newEnd))
{}

void MoveLineDefCommand::execute(editor::EditorState &state)
{
  const auto &lineDef = state.findLinedef(lineDefId);

  auto &startVertex = state.findVertex(editor::getObjectIndex(lineDef.start));
  auto &endVertex = state.findVertex(editor::getObjectIndex(lineDef.end));

  startVertex.x = newStart.x;
  startVertex.y = newStart.y;

  endVertex.x = newEnd.x;
  endVertex.y = newEnd.y;
}

void MoveLineDefCommand::undo(editor::EditorState &state)
{
  const auto &lineDef = state.findLinedef(lineDefId);
  auto &startVertex = state.findVertex(editor::getObjectIndex(lineDef.start));
  auto &endVertex = state.findVertex(editor::getObjectIndex(lineDef.end));

  startVertex.x = oldStart.x;
  startVertex.y = oldStart.y;

  endVertex.x = oldEnd.x;
  endVertex.y = oldEnd.y;
}

AddVertexCommand::AddVertexCommand(editor::EditorVertex _vertex) : vertex(std::move(_vertex)) {}

void AddVertexCommand::execute(editor::EditorState &state)
{
  state.level->vertices.emplace_back(vertex);
}

void AddVertexCommand::undo(editor::EditorState &state)
{
  state.level->vertices.pop_back();
}

void CommandHistory::execute(std::unique_ptr<Command> cmd, editor::EditorState &state)
{
  cmd->execute(state);
  undoStack.emplace_back(std::move(cmd));
  // new action resets the redoStack
  redoStack.clear();
}

void CommandHistory::undo(editor::EditorState &state)
{
  if (undoStack.empty()) return;

  auto lastCmd = std::move(undoStack.back());
  lastCmd->undo(state);
  undoStack.pop_back();

  redoStack.emplace_back(std::move(lastCmd));
}

void CommandHistory::redo(editor::EditorState &state)
{
  if (redoStack.empty()) return;

  auto undoneCommand = std::move(redoStack.back());
  redoStack.pop_back();

  undoneCommand->execute(state);
  undoStack.emplace_back(std::move(undoneCommand));
}
}// namespace commands
