#pragma once
#include "gameloop.h"

#include <array>
#include <fstream>
#include <memory>

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

  int16_t x = 0, y = 0, dx = 0, dy = 0;
  std::array<int16_t, 4> leftBoundingBox{ std::numeric_limits<int16_t>::max(),
    std::numeric_limits<int16_t>::min(),
    std::numeric_limits<int16_t>::min(),
    std::numeric_limits<int16_t>::max() },
    rightBoundingBox{ std::numeric_limits<int16_t>::max(),
      std::numeric_limits<int16_t>::min(),
      std::numeric_limits<int16_t>::min(),
      std::numeric_limits<int16_t>::max() };
  int16_t leftChild = -1, rightChild = -1;
};

inline std::ostream &operator<<(std::ostream &outs, const BspNode &node)
{
  return outs << "BSPNode{ Coords: (" << node.x << ", " << node.y << ", " << node.dx << ", " << node.dy
              << "), Children: (" << node.leftChild << "," << node.rightChild << ")";
}

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
  std::vector<Seg> newSegments;

private:
  SplitResult SplitBySplitter(std::vector<Seg> &segs, const Seg &splitter) const;
  [[nodiscard]] uint32_t SelectSplittingLine(const std::vector<Seg> &segs) const;
  [[nodiscard]] uint32_t EvaluateSplitter(const Seg &seg) const;
  [[nodiscard]] SegmentPosition DetermineSegmentPosition(const Seg &splitter, const Seg &seg) const;
  [[nodiscard]] bool IsConvex(const std::vector<Seg> &segs) const;

  void AdjustBoundingBoxes(const std::vector<Seg> &segs, std::array<int16_t, 4> &boundingBox) const;
  void DrawSubsectors(const SdlWindow &sdlWindow) const;

private:
  static void DrawBoundingBox(const SdlWindow &sdlWindow, const BspNode &root);
  static void DrawSplittingLine(const SdlWindow &sdlWindow, const BspNode &root);

public:
  std::unique_ptr<Level> level;

public:
  explicit BSPBuilder(const Level &_level);

  void BuildBSPTree();
  int BuildBSPTree(std::vector<Seg> &segs);
  void PrintTree() const;
  void Visualize() const;
};
