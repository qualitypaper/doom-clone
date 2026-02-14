#pragma once
#include "gameloop.h"

#include <array>
#include <span>

enum class SegmentPosition { FRONT, BACK, SPANNING };

struct Seg
{
  int16_t startVertex;
  int16_t endVertex;
  int16_t angle;
  int16_t linedefIndex;
  int8_t side;// 0 for front, 1 for back
  int16_t offset;
};

struct BspNode
{
  BspNode() = default;
  BspNode(int16_t _x, int16_t _y, int16_t _dx, int16_t _dy);

  int16_t x=0, y=0, dx=0, dy=0;
  std::array<int16_t, 4> leftBoundingBox{}, rightBoundingBox{};
  int16_t leftChild = -1, rightChild = -1;
};

struct SubSector
{
  int16_t segCount;
  int16_t firstSegIndex;
};

struct SplitResult
{
  BspNode node;
  std::vector<Seg> front;
  std::vector<Seg> back;
};

class BSPBuilder
{
private:
  std::vector<BspNode> nodes;
  std::vector<Seg> segments;
  std::vector<SubSector> subsectors;
  Level &level;

private:
  SplitResult SplitBySplitter(const std::vector<Seg> &segs, const Seg &splitter) const;
  uint32_t SelectSplittingLine(const std::vector<Seg> &segs) const;
  [[nodiscard]] uint32_t EvaluateSplitter(const Seg &seg) const;
  [[nodiscard]] SegmentPosition DetermineSegmentPosition(const Seg &splitter, const Seg &seg) const;
  bool IsConvex(const std::vector<Seg> &segs) const;

public:
  BSPBuilder(Level &_level);

  void BuildBSPTree(const std::vector<Seg> &segs);
};
