#include "core/math_utils.h"
#include "core/serialization/serialization.h"
#include "core/serialization/wad_serializer.h"

#include "bsp/bsp.h"
#include "config.h"
#include "editor.h"
#include "log.h"

#include "ranges"
#include <cstring>

void EditorLineDef::serialize(FileWriter &fw) const
{
  fw.WriteRaw(static_cast<int32_t>(getObjectIndex(start)));
  fw.WriteRaw(static_cast<int32_t>(getObjectIndex(end)));
  const int8_t intType = static_cast<int8_t>(type);
  fw.WriteRaw(intType);

  fw.WriteRaw(static_cast<int16_t>(frontSideDef));
  fw.WriteRaw(static_cast<int16_t>(backSideDef));
}

void EditorLineDef::deserialize(FileReader &fr)
{
  int32_t _start, _end;
  fr.ReadRaw(_start);
  start = makeObjectId(EditorObjectType::VERTEX, _start);
  fr.ReadRaw(_end);
  end = makeObjectId(EditorObjectType::VERTEX, _end);

  int8_t intType;
  fr.ReadRaw(intType);
  type = static_cast<LineDefType>(intType);

  fr.ReadRaw(frontSideDef);
  fr.ReadRaw(backSideDef);
}

void EditorVertex::serialize(FileWriter &fw) const
{
  fw.WriteRaw(x);
  fw.WriteRaw(y);
}

void EditorVertex::deserialize(FileReader &fr)
{
  fr.ReadRaw(x);
  fr.ReadRaw(y);
}


void EditorLevel::SerializeLevelToBuffer(const std::unique_ptr<BspLevel> &bspLevel, std::vector<uint8_t> &buffer) const
{
  VectorWriter writer(buffer);

  writer.WriteVector(bspLevel->linedefs);
  writer.WriteVector(sidedefs);
  writer.WriteVector(bspLevel->vertices);
  writer.WriteVector(bspLevel->segments);
  writer.WriteVector(bspLevel->subsectors);
  writer.WriteVector(bspLevel->nodes);
  writer.WriteVector(sectors);
}

void EditorLevel::SaveLevelToFile(uint16_t &numberOfLevels, const std::unique_ptr<BspLevel> &bspLevel) const
{
  const bool fileExists = std::filesystem::exists(DATA_PATH);

  std::vector<LumpData> allLumps;
  header _header{};
  int32_t targetLevelIndex = -1;
  size_t levelCount = 0;
  // tries to find the last map lump and add the new level at the next position to support the structure of the wad file
  bool found = false;

  if (fileExists) {
    WadSerializer wadSerializer(DATA_PATH);
    if (wadSerializer.Load(true)) {
      _header = wadSerializer.GetHeader();
      allLumps = std::move(wadSerializer.GetLoadedLumps());

      for (size_t i = 0; i < allLumps.size(); ++i) {
        if (!found && allLumps[i].entry.name == this->name) {
          targetLevelIndex = static_cast<int32_t>(i);
          found = true;
        }
        if (memcmp(allLumps[i].entry.name.data(), "Map", 3) == 0) {
          if (!found) {
            targetLevelIndex = static_cast<int32_t>(i);
          }
          levelCount++;
        }
      }
    } else {
      _header = { { 'P', 'W', 'A', 'D' }, 1, 0 };
      levelCount = 1;
    }
  } else {
    _header = { { 'P', 'W', 'A', 'D' }, 1, 0 };
    levelCount = 1;
  }

  std::vector<uint8_t> newLevelBuffer;
  // serializing current level
  SerializeLevelToBuffer(bspLevel, newLevelBuffer);

  LumpData newLevelData;
  newLevelData.rawData = std::move(newLevelBuffer);
  directoryEntry newEntry{};
  std::memcpy(newEntry.name.data(), this->name.data(), newEntry.name.size());
  newLevelData.entry = newEntry;

  if (targetLevelIndex >= 0 && found) {
    allLumps[targetLevelIndex] = std::move(newLevelData);
  } else if (targetLevelIndex >= 0) {
    allLumps.emplace(allLumps.begin() + targetLevelIndex, newLevelData);
  } else {
    allLumps.push_back(std::move(newLevelData));
  }

  WadSerializer wadSerializer(DATA_PATH);
  wadSerializer.SetHeader(_header);
  wadSerializer.SetLumps(std::move(allLumps));
  wadSerializer.Write();

  numberOfLevels = levelCount;
  DOOM_CORE_INFO("Saved Level: {}", 1);
}

void EditorLevel::Save(uint16_t &numberOfLevels)
{
  if (levelNum <= 0) {
    throw std::runtime_error("Level number must be greater than zero.");
  }

  if (numberOfLevels < levelNum) {
    levelNum = numberOfLevels;
  }

  // run bsp algorithm before saving
  auto nativeVertices = vertices | std::views::transform([](const auto &ev) {
                          return Vertex{ DoubleToFixed(ev.x), DoubleToFixed(ev.y) };
                        })
    | std::ranges::to<std::vector<Vertex>>();

  auto nativeLinedefs = linedefs | std::views::transform([](const auto &ld) {
                          return LineDef(ld.start, ld.end, ld.type, ld.frontSideDef, ld.backSideDef);
                        })
    | std::ranges::to<std::vector<LineDef>>();

  BSPBuilder bspBuilder(std::move(nativeVertices), std::move(nativeLinedefs));
  bspBuilder.BuildBSPTree();
  bspBuilder.PrintTree();

  SaveLevelToFile(numberOfLevels, bspBuilder.TakeConstructedLevel());
}


void EditorLevel::Load(const uint16_t width, const uint16_t height)
{
  FileReader fr(DATA_PATH);
  if (!fr.IsStreamGood()) {
    std::cerr << "Failed to open " << DATA_PATH << " for loading. Using hardcoded level data." << '\n';
    return;
  }

  WadSerializer wadSerializer(DATA_PATH);
  if (!wadSerializer.Load(false)) {
    throw std::runtime_error("Failed to read WAD file.");
  }

  const auto entry = wadSerializer.FindEntry(name);
  if (!entry) {
    throw std::runtime_error("Level couldn't be found.");
  }

  fr.SetPos(entry->offset);

  fr.ReadVector(linedefs);
  fr.ReadVector(sidedefs);

  std::vector<Vertex> _vertices;
  fr.ReadVector(_vertices);

  vertices.resize(_vertices.size());
  for (size_t i = 0; i < _vertices.size(); ++i) {
    const Vertex temp = math_utils::fromCenterCoordinates(_vertices[i], width, height);
    vertices[i] = EditorVertex(temp.x, temp.y);
  }


  // skip segments
  size_t segSize = 0;
  fr.ReadRaw(segSize);
  fr.Skip(segSize * sizeof(Seg));

  // skip subsectors
  size_t subsectorSize = 0;
  fr.ReadRaw(subsectorSize);
  fr.Skip(subsectorSize * sizeof(SubSector));

  // skip nodes
  size_t nodesSize = 0;
  fr.ReadRaw(nodesSize);
  fr.Skip(nodesSize * sizeof(Node));

  // read sectors
  fr.ReadVector(sectors);

  std::cout << "Finished loading level from file.\n";
}

/**
 *
 * @param level an uninitialized level object with its name for look up
 * @return number of levels in the file
 */
size_t EditorLevel::Load(std::unique_ptr<Level> &level)
{
  FileReader fr(DATA_PATH);

  WadSerializer wadSerializer(DATA_PATH);
  if (!wadSerializer.Load(false)) {
    throw std::runtime_error("Failed to read WAD file.");
  }

  const auto entry = wadSerializer.FindEntry(level->name);
  if (!entry) {
    throw std::runtime_error("Level couldn't be found.");
  }

  fr.SetPos(entry->offset);

  level->Load(fr);

  size_t numOfLevels = 0;
  for (auto &directoryEntry : wadSerializer.GetDirectory()) {
    if (strncmp((char *)directoryEntry.name.data(), LEVEL_NAME_PREFIX.data(), LEVEL_NAME_PREFIX.size()) == 0) {
      numOfLevels++;
    }
  }
  return numOfLevels;
}
