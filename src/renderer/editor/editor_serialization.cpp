#include "math_utils.h"

#include "bsp.h"
#include "config.h"
#include "editor.h"
#include "serialization.h"

#include "ranges"

void EditorLineDef::serialize(FileWriter &fw) const
{
  fw.WriteRaw(static_cast<int16_t>(getObjectIndex(start)));
  fw.WriteRaw(static_cast<int16_t>(getObjectIndex(end)));
  const int8_t intType = static_cast<int8_t>(type);
  fw.WriteRaw(intType);

  fw.WriteRaw(static_cast<int16_t>(frontSideDef));
  fw.WriteRaw(static_cast<int16_t>(backSideDef));
}

void EditorLineDef::deserialize(FileReader &fr)
{
  int16_t _start, _end;
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

void EditorSector::serialize(FileWriter &fw) const
{
  fw.WriteRaw(floorHeight);
  fw.WriteRaw(ceilingHeight);
  fw.WriteRaw(floorTextureIndex);
  fw.WriteRaw(ceilingTextureIndex);
  fw.WriteRaw(lightLevel);
  fw.WriteRaw(specialType);
  fw.WriteRaw(tag);
  fw.WriteRaw(color);
}

void EditorSector::deserialize(FileReader &fr)
{
  fr.ReadRaw(floorHeight);
  fr.ReadRaw(ceilingHeight);
  fr.ReadRaw(floorTextureIndex);
  fr.ReadRaw(ceilingTextureIndex);
  fr.ReadRaw(lightLevel);
  fr.ReadRaw(specialType);
  fr.ReadRaw(tag);
  fr.ReadRaw(color);
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

void EditorSidedef::serialize(FileWriter &fw) const
{
  fw.WriteRaw(sectorId);
  fw.WriteRaw(upperWallTexture);
  fw.WriteRaw(middleWallTexture);
  fw.WriteRaw(bottomWallTexture);
  fw.WriteRaw(xOffset);
  fw.WriteRaw(yOffset);
}

void EditorSidedef::deserialize(FileReader &fr)
{
  fr.ReadRaw(sectorId);
  fr.ReadRaw(upperWallTexture);
  fr.ReadRaw(middleWallTexture);
  fr.ReadRaw(bottomWallTexture);
  fr.ReadRaw(xOffset);
  fr.ReadRaw(yOffset);
}

void EditorLevel::Save(uint16_t &numberOfLevels, const uint16_t width, const uint16_t height)
{
  if (levelNum <= 0) {
    throw std::runtime_error("Level number must be greater than zero.");
  }

  if (numberOfLevels < levelNum) {
    levelNum = ++numberOfLevels;
  }

  // run bsp algorithm before saving
  auto nativeVertices = vertices | std::views::transform([width, height](const auto &v) {
    const auto vec = math_utils::toCenterCoordinates(v, width, height);
    return Vertex(vec.x, vec.y);
  }) | std::ranges::to<std::vector<Vertex>>();

  auto nativeLinedefs = linedefs | std::views::transform([](const auto &ld) {
    return LineDef(ld.start, ld.end, ld.type, ld.frontSideDef, ld.backSideDef);
  }) | std::ranges::to<std::vector<LineDef>>();

  BSPBuilder bspBuilder(std::move(nativeVertices), std::move(nativeLinedefs));
  bspBuilder.BuildBSPTree();
  bspBuilder.PrintTree();


  // create file if doesn't exist
  std::ios::openmode openmode;
  const bool fileExists = std::filesystem::exists(config::SAVED_LEVEL_PATH);

  if (!fileExists) {
    openmode = std::ios::binary | std::ios::out;
  } else {
    // open in "in" mode in order to avoid truncating the file
    openmode = std::ios::binary | std::ios::out | std::ios::in;
  }

  FileWriter fw(config::SAVED_LEVEL_PATH, openmode);
  fw.SetPos(0);

  std::cout << "Saving level\n";
  std::cout << std::filesystem::current_path() << '\n';

  if (!fw.IsStreamGood()) {
    std::cerr << "Failed to open file for saving." << '\n';
    return;
  }

  // if file is new write file identification
  if (!fileExists) {
    fw.WriteRaw(static_cast<char8_t>('W'));
    fw.WriteRaw(static_cast<char8_t>('A'));
    fw.WriteRaw(static_cast<char8_t>('D'));
  } else {
    fw.Skip(3 * sizeof(char8_t));
  }

  // write number of lumps in the file
  // currently it supports only one level per file
  fw.WriteRaw(static_cast<size_t>(numberOfLevels));

  if (levelNum > 1) {
    std::cout << "Skipping " << levelNum - 1 << " levels in the file.\n";
    FileReader fr(config::SAVED_LEVEL_PATH);
    fr.SetPos(fw.Cursor());

    SkipLevels(fr, levelNum);

    fw.SetPos(fr.Cursor());
  }

  std::unique_ptr<BspLevel> bspLevel = bspBuilder.TakeConstructedLevel();

  const size_t lumpSizePos = fw.Cursor();
  size_t lumpSize = 0;
  fw.WriteRaw(lumpSize);

  fw.WriteVector(bspLevel->linedefs);
  fw.WriteVector(sidedefs);
  fw.WriteVector(bspLevel->vertices);
  fw.WriteVector(bspLevel->segments);
  fw.WriteVector(bspLevel->subsectors);
  fw.WriteVector(bspLevel->nodes);
  fw.WriteVector(sectors);

  lumpSize = fw.Cursor() - lumpSizePos;
  fw.SetPos(lumpSizePos);
  fw.WriteRaw(lumpSize);
  fw.SetPos(fw.Cursor() + lumpSize);
}


/**
 * after calling the method the cursor of the reader is right at the beginning of the data of @param levelNum
 * @param fr initialized FileReader
 * @param levelNum number of the level to skip to
 */
void EditorLevel::SkipLevels(FileReader &fr, uint16_t levelNum)
{
  // if levelNum is already at start, there is no need to skip levels
  if (levelNum <= 1) {
    // skipping the lump size, because the cursor is expected right at the start of the serialized data
    fr.Skip(sizeof(size_t));
    return;
  }

  size_t lumpSize = 0;
  while (fr.IsStreamGood() && levelNum > 1) {
    fr.ReadRaw(lumpSize);
    // read the size of the following lump
    fr.Skip(lumpSize);
    levelNum--;
  }

  // skipping lump size bytes after reaching the needed level
  fr.Skip(sizeof(size_t));
}

size_t EditorLevel::Load(Level &level, const uint16_t levelNum)
{
  FileReader fr(config::SAVED_LEVEL_PATH);

  const size_t numOfLevels = ReadHeaderAndSkipLevels(fr, levelNum);

  level.Load(fr);

  return numOfLevels;
}

size_t EditorLevel::ReadHeaderAndSkipLevels(FileReader &fr, const uint16_t levelNum)
{
  // skip the file identification
  fr.Skip(3 * sizeof(char8_t));

  // read number of levels
  size_t numLevels;
  fr.ReadRaw(numLevels);

  if (levelNum > numLevels) {
    throw std::runtime_error("Level number exceeds the current number of levels.");
  }

  if (!fr.IsStreamGood()) {
    throw std::runtime_error("Stream failed while reading the header.");
  }

  SkipLevels(fr, levelNum);

  if (!fr.IsStreamGood()) {
    throw std::runtime_error("Stream failed while skipping lumps");
  }

  return numLevels;
}

void EditorLevel::Load(const uint16_t width, const uint16_t height)
{
  FileReader fr(config::SAVED_LEVEL_PATH);
  if (!fr.IsStreamGood()) {
    std::cerr << "Failed to open " << config::SAVED_LEVEL_PATH << " for loading. Using hardcoded level data." << '\n';
    return;
  }

  ReadHeaderAndSkipLevels(fr, this->levelNum);

  fr.ReadVector(linedefs);
  std::cout << "Read linedefs\n";

  fr.ReadVector(sidedefs);
  std::cout << "Read sidedefs\n";

  std::vector<Vertex> _vertices;
  fr.ReadVector(_vertices);

  vertices.resize(_vertices.size());
  for (size_t i = 0; i < _vertices.size(); ++i) {
    const Vertex temp = math_utils::fromCenterCoordinates(_vertices[i], width, height);
    vertices[i] = EditorVertex(temp.x, temp.y);
  }

  std::cout << "Read vertices\n";

  // skip segments
  size_t segSize = 0;
  fr.ReadRaw(segSize);
  fr.Skip(segSize * Seg::SerializationSize());

  // skip subsectors
  size_t subsectorSize = 0;
  fr.ReadRaw(subsectorSize);
  fr.Skip(subsectorSize * SubSector::SerializationSize());

  // skip nodes
  size_t nodesSize = 0;
  fr.ReadRaw(nodesSize);
  fr.Skip(nodesSize * BspNode::SerializationSize());

  // read sectors
  fr.ReadVector(sectors);
  std::cout << "Read sectors\n";

  std::cout << "Finished loading level from file.\n";
}

void EditorLevel::toGameLevel(Level &level, const uint16_t width, const uint16_t height) const
{
  level.vertices.clear();
  level.linedefs.clear();
  level.sidedefs.clear();
  level.sectors.clear();

  level.vertices.reserve(vertices.size());
  level.linedefs.reserve(linedefs.size());
  level.sidedefs.reserve(sidedefs.size());
  level.sectors.reserve(sectors.size());

  for (auto &v : vertices) {
    EditorVertex res = math_utils::toCenterCoordinates(v, width, height);
    level.vertices.emplace_back(res.x, res.y);
  }

  for (auto &ld : linedefs) {
    level.linedefs.emplace_back(
      getObjectIndex(ld.start), getObjectIndex(ld.end), ld.type, ld.frontSideDef, ld.backSideDef);
  }

  for (auto &sd : sidedefs) {
    level.sidedefs.emplace_back(
      sd.sectorId, sd.xOffset, sd.yOffset, sd.upperWallTexture, sd.middleWallTexture, sd.bottomWallTexture);
  }

  for (auto &sector : sectors) {
    level.sectors.emplace_back(sector.floorHeight,
      sector.ceilingHeight,
      sector.floorTextureIndex,
      sector.ceilingTextureIndex,
      sector.specialType,
      sector.lightLevel,
      sector.tag,
      sector.color);
  }
}