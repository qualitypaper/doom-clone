#include "commands.h"

AddLineDefCommand::AddLineDefCommand(EditorLineDef _lineDef) : lineDef(std::move(_lineDef)) {}

void AddLineDefCommand::execute(EditorState &state)
{
  state.level->linedefs.emplace_back(lineDef);
  const size_t ldIndex = state.level->linedefs.size() - 1;
  state.findVertex(lineDef.start).connectedLineDefs.push_back(ldIndex);
  state.findVertex(lineDef.end).connectedLineDefs.push_back(ldIndex);
}

void AddLineDefCommand::undo(EditorState &state)
{
  state.level->linedefs.pop_back();
  state.findVertex(lineDef.start).connectedLineDefs.pop_back();
  state.findVertex(lineDef.end).connectedLineDefs.pop_back();
}

MoveVertexCommand::MoveVertexCommand(const uint32_t _vertexId, EditorVertex _oldVertex, EditorVertex _newVertex)
  : vertexId(_vertexId), oldVertex(std::move(_oldVertex)), newVertex(std::move(_newVertex))
{}

void MoveVertexCommand::execute(EditorState &state)
{
  auto &vertex = state.findVertex(vertexId);
  vertex.x = newVertex.x;
  vertex.y = newVertex.y;
}

void MoveVertexCommand::undo(EditorState &state)
{
  auto &vertex = state.findVertex(vertexId);
  vertex.x = oldVertex.x;
  vertex.y = oldVertex.y;
}

MoveLineDefCommand::MoveLineDefCommand(uint32_t _lineDefId,
  EditorVertex _oldStart,
  EditorVertex _oldEnd,
  EditorVertex _newStart,
  EditorVertex _newEnd)
  : lineDefId(_lineDefId), oldStart(std::move(_oldStart)), oldEnd(std::move(_oldEnd)), newStart(std::move(_newStart)),
    newEnd(std::move(_newEnd))
{}

void MoveLineDefCommand::execute(EditorState &state)
{
  const auto &lineDef = state.findLinedef(lineDefId);

  auto &startVertex = state.findVertex(getObjectIndex(lineDef.start));
  auto &endVertex = state.findVertex(getObjectIndex(lineDef.end));

  startVertex.x = newStart.x;
  startVertex.y = newStart.y;

  endVertex.x = newEnd.x;
  endVertex.y = newEnd.y;
}

void MoveLineDefCommand::undo(EditorState &state)
{
  const auto &lineDef = state.findLinedef(lineDefId);
  auto &startVertex = state.findVertex(getObjectIndex(lineDef.start));
  auto &endVertex = state.findVertex(getObjectIndex(lineDef.end));

  startVertex.x = oldStart.x;
  startVertex.y = oldStart.y;

  endVertex.x = oldEnd.x;
  endVertex.y = oldEnd.y;
}

AddVertexCommand::AddVertexCommand(EditorVertex _vertex) : vertex(std::move(_vertex)) {}

void AddVertexCommand::execute(EditorState &state) { state.level->vertices.emplace_back(vertex); }

void AddVertexCommand::undo(EditorState &state) { state.level->vertices.pop_back(); }

void CommandHistory::execute(std::unique_ptr<Command> cmd, EditorState &state)
{
  cmd->execute(state);
  undoStack.emplace_back(std::move(cmd));
  // new action resets the redoStack
  redoStack.clear();
}

void CommandHistory::undo(EditorState &state)
{
  if (undoStack.empty()) return;

  auto lastCmd = std::move(undoStack.back());
  lastCmd->undo(state);
  undoStack.pop_back();

  redoStack.emplace_back(std::move(lastCmd));
}

void CommandHistory::redo(EditorState &state)
{
  if (redoStack.empty()) return;

  auto undoneCommand = std::move(redoStack.back());
  redoStack.pop_back();

  undoneCommand->execute(state);
  undoStack.emplace_back(std::move(undoneCommand));
}
