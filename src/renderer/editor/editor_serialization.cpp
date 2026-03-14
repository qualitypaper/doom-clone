#include "math_utils.h"

#include "bsp.h"
#include "editor.h"
#include "serialization.h"

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

void EditorLevel::save(const uint16_t width, const uint16_t height) const
{
  // run bsp algorithm before saving
  //Level level;
  //toGameLevel(level, width, height);

  //BSPBuilder bspBuilder();

  FileWriter fw("saved_level.bin");

  std::cout << "Saving level to saved_level.bin...\n";
  std::cout << std::filesystem::current_path() << '\n';

  if (!fw.IsStreamGood()) {
    std::cerr << "Failed to open file for saving." << std::endl;
    return;
  }

  // write vertices
  fw.WriteVector(vertices);

  // write linedefs
  fw.WriteVector(linedefs);

  // write sidedefs
  fw.WriteVector(sidedefs);

  // write sectors
  fw.WriteVector(sectors);
}

void EditorLevel::load()
{
  FileReader fr("saved_level.bin");
  if (!fr.IsStreamGood()) {
    std::cerr << "Failed to open saved_level.bin for loading. Using hardcoded level data." << std::endl;
    return;
  }

  fr.ReadVector(vertices);
  std::cout << "Read vertices\n";

  fr.ReadVector(linedefs);
  std::cout << "Read linedefs\n";

  fr.ReadVector(sidedefs);
  std::cout << "Read sidedefs\n";

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