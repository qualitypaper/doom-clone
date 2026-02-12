#include "editor.h"

#include "commands.h"
#include "editor_input_handler.h"
#include "gameloop.h"
#include "math_utils.h"

#include "algorithm"

#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace editor {

AABB::AABB(gameloop::Vertex start, gameloop::Vertex end)
{
  this->maxX = std::max(start.x, end.x);
  this->minX = std::min(start.x, end.x);
  this->maxY = std::max(start.y, end.y);
  this->minY = std::min(start.y, end.y);
}

void EditorLevel::serialize(const char *filename,
  uint16_t width,
  uint16_t height,
  uint16_t canvasWidth,
  uint16_t canvasHeight) const
{
  std::ofstream file(filename, std::ios::binary);

  std::cout << "Saving level to saved_level.bin...\n";
  std::cout << std::filesystem::current_path() << '\n';

  if (!file || !file.is_open() || file.fail()) {
    std::cerr << "Failed to open file for saving." << std::endl;
    return;
  }

  // write vertices
  uint64_t verticesCount = vertices.size();
  uint64_t linedefsCount = linedefs.size();
  uint64_t sidedefsCount = sidedefs.size();
  uint64_t sectorsCount = sectors.size();
  file.write(reinterpret_cast<char *>(&verticesCount), sizeof(verticesCount));
  for (auto &v : vertices) {
    auto [x, y] = math_utils::toCenterCoordinates(gameloop::Vertex{ v.x, v.y }, width, height);
    const EditorVertex canvasVertexInt{ static_cast<int16_t>(y), static_cast<int16_t>(x) };
    file.write(reinterpret_cast<const char *>(&canvasVertexInt.x), sizeof(canvasVertexInt.x));
    file.write(reinterpret_cast<const char *>(&canvasVertexInt.y), sizeof(canvasVertexInt.y));
  }

  // write linedefs
  file.write(reinterpret_cast<char *>(&linedefsCount), sizeof(linedefsCount));
  const auto fitsInt16 = [](int32_t value) {
    return value >= std::numeric_limits<int16_t>::min() && value <= std::numeric_limits<int16_t>::max();
  };
  for (auto &linedef : linedefs) {
    if (linedef.start > static_cast<uint32_t>(std::numeric_limits<int16_t>::max())
        || linedef.end > static_cast<uint32_t>(std::numeric_limits<int16_t>::max()) || !fitsInt16(linedef.frontSideDef)
        || !fitsInt16(linedef.backSideDef)) {
      std::cerr << "Linedef index out of int16_t range during serialization." << std::endl;
      return;
    }

    const int16_t start = static_cast<int16_t>(linedef.start);
    const int16_t end = static_cast<int16_t>(linedef.end);
    const int32_t typeValue = static_cast<int32_t>(linedef.type);
    const int16_t front = static_cast<int16_t>(linedef.frontSideDef);
    const int16_t back = static_cast<int16_t>(linedef.backSideDef);

    file.write(reinterpret_cast<const char *>(&start), sizeof(start));
    file.write(reinterpret_cast<const char *>(&end), sizeof(end));
    file.write(reinterpret_cast<const char *>(&typeValue), sizeof(typeValue));
    file.write(reinterpret_cast<const char *>(&front), sizeof(front));
    file.write(reinterpret_cast<const char *>(&back), sizeof(back));
  }

  // write sidedefs
  file.write(reinterpret_cast<char *>(&sidedefsCount), sizeof(sidedefsCount));
  for (auto &[sectorId, xOffset, yOffset] : sidedefs) {
    file.write(reinterpret_cast<const char *>(&sectorId), sizeof(sectorId));
    file.write(reinterpret_cast<const char *>(&xOffset), sizeof(xOffset));
    file.write(reinterpret_cast<const char *>(&yOffset), sizeof(yOffset));
  }

  // write sectors
  file.write(reinterpret_cast<const char *>(&sectorsCount), sizeof(sectorsCount));
  for (const auto &sector : sectors) {
    file.write(reinterpret_cast<const char *>(&sector.floorHeight), sizeof(sector.floorHeight));
    file.write(reinterpret_cast<const char *>(&sector.ceilingHeight), sizeof(sector.ceilingHeight));
    file.write(reinterpret_cast<const char *>(&sector.specialType), sizeof(sector.specialType));
    file.write(reinterpret_cast<const char *>(&sector.lightLevel), sizeof(sector.lightLevel));
    file.write(reinterpret_cast<const char *>(&sector.tag), sizeof(sector.tag));
  }

  file.flush();
  file.close();
}

void EditorLevel::deserialize(gameloop::Level &level, const char *filename)
{
  std::ifstream in(filename, std::ios::binary);
  if (!in || !in.is_open()) {
    std::cerr << "Failed to open saved_level.bin for loading. Using hardcoded level data." << std::endl;
    return;
  }

  level.vertices.clear();
  level.linedefs.clear();
  level.sidedefs.clear();
  level.sectors.clear();

  uint64_t verticesCount, linedefsCount, sidedefsCount, sectorsCount;

  in.read(reinterpret_cast<char *>(&verticesCount), sizeof(verticesCount));
  for (size_t i = 0; i < verticesCount; i++) {
    gameloop::Vertex v{};
    in.read(reinterpret_cast<char *>(&v.x), sizeof(v.x));
    in.read(reinterpret_cast<char *>(&v.y), sizeof(v.y));
    level.vertices.emplace_back(v);
  }
  if (in.fail()) return;
  std::cout << "Read vertices\n";

  in.read(reinterpret_cast<char *>(&linedefsCount), sizeof(linedefsCount));
  for (size_t i = 0; i < linedefsCount; i++) {
    gameloop::LineDef ld{};
    int16_t start = 0;
    int16_t end = 0;
    int32_t typeValue = 0;
    int16_t front = 0;
    int16_t back = 0;

    in.read(reinterpret_cast<char *>(&start), sizeof(start));
    in.read(reinterpret_cast<char *>(&end), sizeof(end));
    in.read(reinterpret_cast<char *>(&typeValue), sizeof(typeValue));
    in.read(reinterpret_cast<char *>(&front), sizeof(front));
    in.read(reinterpret_cast<char *>(&back), sizeof(back));

    ld.start = start;
    ld.end = end;
    ld.type = static_cast<gameloop::LineDefType>(typeValue);
    ld.frontSidedef = front;
    ld.backSidedef = back;

    level.linedefs.emplace_back(ld);
  }
  if (in.fail()) return;
  std::cout << "Read linedefs\n";

  in.read(reinterpret_cast<char *>(&sidedefsCount), sizeof(sidedefsCount));
  for (size_t i = 0; i < sidedefsCount; i++) {
    gameloop::SideDef sd;
    in.read(reinterpret_cast<char *>(&sd.sectorId), sizeof(sd.sectorId));
    in.read(reinterpret_cast<char *>(&sd.xOffset), sizeof(sd.xOffset));
    in.read(reinterpret_cast<char *>(&sd.yOffset), sizeof(sd.yOffset));
    level.sidedefs.emplace_back(sd);
  }
  if (in.fail()) return;
  std::cout << "Read sidedefs\n";

  in.read(reinterpret_cast<char *>(&sectorsCount), sizeof(sectorsCount));
  for (size_t i = 0; i < sectorsCount; i++) {
    gameloop::Sector sec{};
    in.read(reinterpret_cast<char *>(&sec.floorHeight), sizeof(sec.floorHeight));
    in.read(reinterpret_cast<char *>(&sec.ceilingHeight), sizeof(sec.ceilingHeight));
    in.read(reinterpret_cast<char *>(&sec.specialType), sizeof(sec.specialType));
    in.read(reinterpret_cast<char *>(&sec.lightLevel), sizeof(sec.lightLevel));
    in.read(reinterpret_cast<char *>(&sec.tag), sizeof(sec.tag));
    level.sectors.emplace_back(sec);
  }
  if (in.fail()) {
    std::cerr << "Failed to read sectors from file." << std::endl;
    return;
  }
  std::cout << "Read sectors\n";
  std::cout << "Finished loading level from file.\n";
}

bool EditorVertex::isAnyConnectedLineDefSelected(const EditorState &state) const
{
  return std::ranges::any_of(
    connectedLineDefs, [&](auto ldObjectId) { return state.findLinedef(getObjectIndex(ldObjectId)).selected; });
}

void EditorLineDef::remove(const EditorState &state, const uint32_t ldId)
{
  const uint32_t lastLdIndex = static_cast<uint32_t>(state.level->linedefs.size() - 1);
  auto &ld = state.level->linedefs[ldId];
  auto &lastLd = state.level->linedefs.back();
  auto &vStart = state.findVertex(ld.start);
  auto &vEnd = state.findVertex(ld.end);
  auto &lastLdStart = state.findVertex(lastLd.start);
  auto &lastLdEnd = state.findVertex(lastLd.end);

  if (ldId != lastLdIndex) {
    // Update indices in connected vertices
    for (auto &ldObjectId : lastLdStart.connectedLineDefs) {
      if (getObjectIndex(ldObjectId) == lastLdIndex) {
        ldObjectId = makeObjectId(EditorObjectType::LINEDEF, ldId);
        break;
      }
    }
    for (auto &ldObjectId : lastLdEnd.connectedLineDefs) {
      if (getObjectIndex(ldObjectId) == lastLdIndex) {
        ldObjectId = makeObjectId(EditorObjectType::LINEDEF, ldId);
        break;
      }
    }

    ld = lastLd;
  }
  std::erase_if(
    vStart.connectedLineDefs, [ldId](const auto &ldObjectId) { return getObjectIndex(ldObjectId) == ldId; });
  std::erase_if(vEnd.connectedLineDefs, [ldId](const auto &ldObjectId) { return getObjectIndex(ldObjectId) == ldId; });

  state.level->linedefs.pop_back();
}

void EditorVertex::remove(EditorState &state, const uint32_t vertexId)
{
  auto &vertex = state.level->vertices[vertexId];
  // Remove all linedefs that reference this vertex (iterate backwards to avoid index issues)
  for (int i = static_cast<int>(vertex.connectedLineDefs.size()) - 1; i >= 0; i--) {
    uint32_t ldId = getObjectIndex(vertex.connectedLineDefs[i]);
    const auto &ld = state.level->linedefs[ldId];

    if (ld.start == vertexId) {
      auto &vEnd = state.findVertex(ld.end);

      std::erase_if(
        vEnd.connectedLineDefs, [&ldId](const auto &ldObjectId) { return editor::getObjectIndex(ldObjectId) == ldId; });
    } else if (ld.end == vertexId) {
      auto &vStart = state.findVertex(ld.start);

      std::erase_if(vStart.connectedLineDefs,
        [&ldId](const auto &ldObjectId) { return editor::getObjectIndex(ldObjectId) == ldId; });
    }

    EditorLineDef::remove(state, ldId);
  }

  // Remove the vertex using swap-and-pop
  const auto lastVertexIndex = static_cast<uint32_t>(state.level->vertices.size() - 1);

  if (vertexId != lastVertexIndex) {
    const auto &lastVertex = state.level->vertices.back();

    for (const uint32_t ldObjectId : lastVertex.connectedLineDefs) {
      const auto ldIndex = editor::getObjectIndex(ldObjectId);
      auto &ld = state.findLinedef(ldIndex);

      if (ld.start == lastVertexIndex) ld.start = vertexId;
      if (ld.end == lastVertexIndex) ld.end = vertexId;
    }

    vertex = lastVertex;
  }

  state.selection.clear();
  state.level->vertices.pop_back();
}

EditorState::EditorState(gameloop::Level &_level,
  uint16_t _width,
  uint16_t _height,
  uint16_t _canvasWidth,
  uint16_t _canvasHeight)
  : width(_width), height(_height), canvasWidth(_canvasWidth), canvasHeight(_canvasHeight)
{
  std::vector<EditorVertex> editorVertices;
  std::vector<EditorLineDef> editorLinedefs;
  std::vector<EditorSector> editorSectors;

  editorVertices.reserve(_level.vertices.size());
  editorLinedefs.reserve(_level.linedefs.size());
  editorSectors.reserve(_level.sectors.size());

  std::unordered_map<size_t, uint32_t> vertexIdMap;
  vertexIdMap.reserve(_level.vertices.size());

  // process vertices
  for (size_t i = 0; i < _level.vertices.size(); ++i) {
    const auto &v = _level.vertices[i];

    auto convertedImvec2 = math_utils::fromCenterCoordinates(math_utils::convertVertexIntoImVec2(v), _width, _height);
    editorVertices.emplace_back(convertedImvec2.x, convertedImvec2.y);
    vertexIdMap[i] = editorVertices.size() - 1;
  }

  // process linedefs
  for (const auto &[start, end, type, frontSidedef, backSidedef] : _level.linedefs) {
    editorLinedefs.emplace_back(vertexIdMap[start], vertexIdMap[end], type, frontSidedef, backSidedef);

    editorVertices[vertexIdMap[start]].connectedLineDefs.emplace_back(
      makeObjectId(EditorObjectType::LINEDEF, static_cast<uint32_t>(editorLinedefs.size() - 1)));
    editorVertices[vertexIdMap[end]].connectedLineDefs.emplace_back(
      makeObjectId(EditorObjectType::LINEDEF, static_cast<uint32_t>(editorLinedefs.size() - 1)));
  }

  // process sectors
  for (const auto &[floorHeight, ceilingHeight, specialType, lightLevel, tag] : _level.sectors) {
    editorSectors.emplace_back();
    auto &sec = editorSectors.back();
    sec.floorHeight = floorHeight;
    sec.ceilingHeight = ceilingHeight;
    sec.specialType = specialType;
    sec.lightLevel = lightLevel;
    sec.tag = tag;
  }

  this->level = std::make_unique<EditorLevel>(
    std::move(editorVertices), std::move(editorLinedefs), std::move(editorSectors), _level.sidedefs);
}

void EditorState::reset()
{
  for (auto &vertex : level->vertices) {
    vertex.selected = false;
    vertex.hovered = false;
  }

  for (auto &linedef : level->linedefs) {
    linedef.selected = false;
    linedef.hovered = false;
  }

  for (auto &sector : level->sectors) {
    sector.selected = false;
    sector.hovered = false;
  }

  selection.clear();

  isDragging = false;
  draggingOffset = { 0, 0 };
  draggingStart = { 0, 0 };

  isCreatingLine = false;
  lineStartVertexId = 0;

  isBlockSelecting = false;
  blockSelectionStart = { 0, 0 };

  renderOptionsWindow = false;
  optionsWindowPos = { 0, 0 };

  canvasOrigin = { 0.0f, 0.0f };
  canvasScroll = { 0.0f, 0.0f };
  canvasZoom = 1.0f;
}

void Editor::updateAABB(uint32_t sectorID)
{
  EditorSector &sec = this->state.get()->findSector(sectorID);

  // set values to max/min double values, so the first vertex overrides them
  sec.bounding_box.minX = std::numeric_limits<int16_t>::max();
  sec.bounding_box.minY = std::numeric_limits<int16_t>::max();
  sec.bounding_box.maxX = std::numeric_limits<int16_t>::lowest();
  sec.bounding_box.maxY = std::numeric_limits<int16_t>::lowest();

  for (int16_t ldId : sec.linedefIds) {
    const auto &ld = state.get()->findLinedef(ldId);

    const auto &start = state.get()->findVertex(ld.start);
    const auto &end = state.get()->findVertex(ld.end);

    // check start
    if (start.x < sec.bounding_box.minX) sec.bounding_box.minX = start.x;
    if (start.x > sec.bounding_box.maxX) sec.bounding_box.maxX = start.x;

    if (start.y < sec.bounding_box.minY) sec.bounding_box.minY = start.y;
    if (start.y > sec.bounding_box.maxY) sec.bounding_box.maxY = start.y;

    // check end
    if (end.x < sec.bounding_box.minX) sec.bounding_box.minX = end.x;
    if (end.x > sec.bounding_box.maxX) sec.bounding_box.maxX = end.x;

    if (end.y < sec.bounding_box.minY) sec.bounding_box.minY = end.y;
    if (end.y > sec.bounding_box.maxY) sec.bounding_box.maxY = end.y;
  }
}

Editor::Editor(gameloop::Level &_level,
  uint16_t _width,
  uint16_t _height,
  uint16_t _canvasWidth,
  uint16_t _canvasHeight)
  : m_inputHandler(std::make_unique<EditorInputHandler>()), m_history(std::make_unique<commands::CommandHistory>()),
    state(std::make_unique<EditorState>(_level, _width, _height, _canvasWidth, _canvasHeight))
{}

void Editor::processInput(const float_t vertexRadius) const
{ EditorInputHandler::processInput(*this->state, *this->m_history, vertexRadius); }

void Editor::addLineDef(int32_t sectorId, gameloop::LineDef &linedef)
{
  state->level->linedefs.emplace_back(linedef);

  if (sectorId == -1) return;

  auto sector = state->findSector(sectorId);

  sector.linedefIds.emplace_back();
  this->updateAABB(sectorId);
}

void Editor::addLineDef(gameloop::Vertex start, gameloop::Vertex end)
{
  // level.vertices.emplace_back(start);
  // level.vertices.emplace_back(end);

  // gameloop::LineDef ld{ .start = static_cast<int16_t>(level.vertices.size() - 2),
  //   .end = static_cast<int16_t>(level.vertices.size() - 1),
  //   .type = gameloop::LineDefType::REGULAR,
  //   .frontSidedef = -1,
  //   .backSidedef = -1 };

  // level.linedefs.emplace_back(ld);
}

void Editor::addVertex(const int16_t x, const int16_t y) const
{
  m_history->execute(std::make_unique<commands::AddVertexCommand>(EditorVertex(x, y)), *state);


  float_t dMin1 = std::numeric_limits<float_t>::max(), dMin2 = std::numeric_limits<float_t>::max();
  int16_t firstVertexIdx, secondVertexIdx;

  // find nearest two vertices with which to connect a vertex
  // auto &sector = state->findSector(sectorId);
  //
  // for (const uint32_t ldIndex : sector.linedefIds) {
  //   auto &ld = state->findLinedef(ldIndex);
  //
  //   auto &start = state->findVertex(ld.start);
  //   auto &end = state->findVertex(ld.end);
  //
  //   float_t d1 = math_utils::getDistanceSq(start.x, start.y, x, y);
  //   float_t d2 = math_utils::getDistanceSq(end.x, end.y, x, y);
  //
  //   if (d1 < dMin1) {
  //     dMin2 = dMin1;
  //     dMin1 = d1;
  //     secondVertexIdx = firstVertexIdx;
  //     firstVertexIdx = ld.start;
  //   } else if (d1 < dMin2) {
  //     dMin2 = d1;
  //     secondVertexIdx = ld.start;
  //   }
  //
  //   if (d2 < dMin1) {
  //     dMin2 = dMin1;
  //     dMin1 = d2;
  //     secondVertexIdx = firstVertexIdx;
  //     firstVertexIdx = ld.end;
  //   } else if (d2 < dMin2) {
  //     dMin2 = d2;
  //     secondVertexIdx = ld.end;
  //   }
  // }
}

void Editor::executeCommand(std::unique_ptr<commands::Command> cmd) const
{ m_history->execute(std::move(cmd), *state); }


void Editor::drawConnectedLine(const uint32_t vertexIndex) const
{
  state->isCreatingLine = true;
  state->lineStartVertexId = vertexIndex;
}

}// namespace editor