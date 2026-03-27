#include "math_utils.h"

#include "bsp.h"
#include "editor.h"
#include "serialization.h"

#include "ranges"

void EditorLineDef::serialize(FileWriter &fw) const
{
  fw.WriteRaw(getObjectIndex(start));
  fw.WriteRaw(getObjectIndex(end));
  const int32_t intType = static_cast<int32_t>(type);
  fw.WriteRaw(intType);
  fw.WriteRaw(frontSideDef);
  fw.WriteRaw(backSideDef);
}

void EditorLineDef::deserialize(FileReader &fr)
{
  fr.ReadRaw(start);
  start = makeObjectId(EditorObjectType::VERTEX, start);
  fr.ReadRaw(end);
  end = makeObjectId(EditorObjectType::VERTEX, end);

  int32_t intType;
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

void EditorLevel::save(uint16_t &levelsNum, const uint16_t width, const uint16_t height)
{
  if (levelNum <= 0) {
    throw std::runtime_error("Level number must be greater than zero.");
  }

  if (levelsNum < levelNum) {
    levelNum = ++levelsNum;
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

  FileWriter fw("saved_level.wad");

  std::cout << "Saving level to saved_level.wad \n";
  std::cout << std::filesystem::current_path() << '\n';

  if (!fw.IsStreamGood()) {
    std::cerr << "Failed to open file for saving." << '\n';
    return;
  }

  // write identification
  fw.WriteRaw((char8_t)'W');
  fw.WriteRaw((char8_t)'A');
  fw.WriteRaw((char8_t)'D');

  // write number of lumps in the file
  // currently it supports only one level per file
  fw.WriteRaw((size_t)levelsNum);

  if (levelNum > 1) {
    std::cout << "Skipping " << levelNum - 1 << " levels in the file.\n";
    FileReader fr("saved_level.wad");
    SkipLevels(fr, levelNum);
  }

  std::unique_ptr<BspLevel> bspLevel = bspBuilder.TakeConstructedLevel();

  size_t lumpSize = 0;

  lumpSize += bspLevel->linedefs.size() * sizeof(LineDef);
  lumpSize += sidedefs.size() * sizeof(SideDef);
  lumpSize += bspLevel->vertices.size() * sizeof(Vertex);
  lumpSize += bspLevel->segments.size() * sizeof(Seg);
  lumpSize += bspLevel->subsectors.size() * sizeof(SubSector);
  lumpSize += bspLevel->nodes.size() * sizeof(BspNode);
  lumpSize += sectors.size() * sizeof(Sector);

  fw.WriteRaw(lumpSize);

  // write linedefs
  fw.WriteVector(std::move(bspLevel->linedefs));

  // write sidedefs
  fw.WriteVector(sidedefs);

  // write vertices
  fw.WriteVector(std::move(bspLevel->vertices));

  // write segs
  fw.WriteVector(std::move(bspLevel->segments));

  // write subsectors
  fw.WriteVector(std::move(bspLevel->subsectors));

  // write nodes
  fw.WriteVector(std::move(bspLevel->nodes));

  // write sectors
  fw.WriteVector(sectors);
}

void EditorLevel::SkipLevels(FileReader &fr, uint16_t levelNum)
{
  size_t lumpSize = 0;
  fr.ReadRaw(lumpSize);

  while (fr.IsStreamGood() && levelNum > 1) {
    // read the size of the following lump
    fr.Skip(lumpSize);
    levelNum--;
  }
}

void EditorLevel::Load(Level &level, const uint16_t levelNum)
{
  FileReader fr("saved_level.wad");

  SkipHeaderAndLevels(fr, levelNum);

  level.Load(fr);
}

void EditorLevel::SkipHeaderAndLevels(FileReader &fr, const uint16_t levelNum)
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
}

void EditorLevel::Load(const uint16_t width, const uint16_t height)
{
  FileReader fr("saved_level.wad");
  if (!fr.IsStreamGood()) {
    std::cerr << "Failed to open saved_level.wad for loading. Using hardcoded level data." << std::endl;
    return;
  }

  SkipHeaderAndLevels(fr, this->levelNum);

  fr.ReadVector(linedefs);
  std::cout << "Read linedefs\n";

  fr.ReadVector(sidedefs);
  std::cout << "Read sidedefs\n";

  fr.ReadVector(vertices);
  std::cout << "Read vertices\n";

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
  fr.Skip(nodesSize * sizeof(BspNode));

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