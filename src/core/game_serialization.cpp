#include "bsp.h"
#include "gameloop.h"
#include "serialization.h"

void Vertex::serialize(FileWriter &fw) const
{
  fw.WriteRaw(x);
  fw.WriteRaw(y);
}

void Vertex::deserialize(FileReader &fr)
{
  fr.ReadRaw(x);
  fr.ReadRaw(y);
}

void LineDef::serialize(FileWriter &fw) const
{
  fw.WriteRaw(start);
  fw.WriteRaw(end);
  const int8_t intType = static_cast<int8_t>(type);

  fw.WriteRaw(intType);
  fw.WriteRaw(frontSidedef);
  fw.WriteRaw(backSidedef);
}

void LineDef::deserialize(FileReader &fr)
{
  fr.ReadRaw(start);
  fr.ReadRaw(end);
  int8_t intType;
  fr.ReadRaw(intType);
  type = static_cast<LineDefType>(intType);

  fr.ReadRaw(frontSidedef);
  fr.ReadRaw(backSidedef);
}

void SideDef::serialize(FileWriter &fw) const
{
  fw.WriteRaw(sectorId);
  fw.WriteRaw(upperWallTexture);
  fw.WriteRaw(middleWallTexture);
  fw.WriteRaw(bottomWallTexture);
  fw.WriteRaw(xOffset);
  fw.WriteRaw(yOffset);
}

void SideDef::deserialize(FileReader &fr)
{
  fr.ReadRaw(sectorId);
  fr.ReadRaw(upperWallTexture);
  fr.ReadRaw(middleWallTexture);
  fr.ReadRaw(bottomWallTexture);
  fr.ReadRaw(xOffset);
  fr.ReadRaw(yOffset);
}

void Sector::serialize(FileWriter &fw) const
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

void Sector::deserialize(FileReader &fr)
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

void BspNode::serialize(FileWriter &fw) const
{
  fw.WriteRaw(x);
  fw.WriteRaw(y);
  fw.WriteRaw(dx);
  fw.WriteRaw(dy);

  fw.WriteRaw(leftBoundingBox);
  fw.WriteRaw(rightBoundingBox);

  fw.WriteRaw(leftChild);
  fw.WriteRaw(rightChild);
}

size_t BspNode::SerializationSize()
{
  return sizeof(x) + sizeof(y) + sizeof(dx) + sizeof(dy) + sizeof(leftBoundingBox) + sizeof(rightBoundingBox)
    + sizeof(leftChild) + sizeof(rightChild);
}

void BspNode::deserialize(FileReader &fr)
{
  fr.ReadRaw(x);
  fr.ReadRaw(y);
  fr.ReadRaw(dx);
  fr.ReadRaw(dy);

  fr.ReadRaw(leftBoundingBox);
  fr.ReadRaw(rightBoundingBox);

  fr.ReadRaw(leftChild);
  fr.ReadRaw(rightChild);
}

void Seg::serialize(FileWriter &fw) const
{
  fw.WriteRaw(startVertex);
  fw.WriteRaw(endVertex);
  fw.WriteRaw(angle);
  fw.WriteRaw(linedefIndex);
  fw.WriteRaw(side);
  fw.WriteRaw(offset);
}

void Seg::deserialize(FileReader &fr)
{
  fr.ReadRaw(startVertex);
  fr.ReadRaw(endVertex);
  fr.ReadRaw(angle);
  fr.ReadRaw(linedefIndex);
  fr.ReadRaw(side);
  fr.ReadRaw(offset);
}
size_t Seg::SerializationSize()
{
  return sizeof(startVertex) + sizeof(endVertex) + sizeof(angle) + sizeof(linedefIndex) + sizeof(side) + sizeof(offset);
}

void SubSector::serialize(FileWriter &fw) const
{
  fw.WriteRaw(segCount);
  fw.WriteRaw(firstSegIndex);
}

size_t SubSector::SerializationSize() { return sizeof(segCount) + sizeof(firstSegIndex); }

void SubSector::deserialize(FileReader &fr)
{
  fr.ReadRaw(segCount);
  fr.ReadRaw(firstSegIndex);
}
