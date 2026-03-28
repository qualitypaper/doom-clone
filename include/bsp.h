#pragma once
#include "gameloop.h"
#include "serialization.h"

#include <array>
#include <fstream>
#include <memory>
#include <span>

enum class SegmentPosition { FRONT, BACK, SPANNING };

struct Seg
{
  int16_t startVertex = -1;
  int16_t endVertex = -1;
  int16_t angle = 0;
  int16_t linedefIndex = -1;
  int16_t side = -1;// 0 for front, 1 for back
  int16_t offset = 0;

  Seg() = default;
  Seg(int16_t _startVertex, int16_t _endVertex, int16_t _angle, int16_t _linedefIndex, int8_t _side, int16_t _offset)
    : startVertex(_startVertex), endVertex(_endVertex), angle(_angle), linedefIndex(_linedefIndex), side(_side),
      offset(_offset)

  {}

  bool operator==(const Seg &other) const
  {
    return startVertex == other.startVertex && endVertex == other.endVertex && linedefIndex == other.linedefIndex;
  }

  template<typename Writer> void serialize(Writer &w) const
  {
    w.WriteRaw(startVertex);
    w.WriteRaw(endVertex);
    w.WriteRaw(angle);
    w.WriteRaw(linedefIndex);
    w.WriteRaw(side);
    w.WriteRaw(offset);
  }

  template<typename Reader> void deserialize(Reader &r)
  {
    r.ReadRaw(startVertex);
    r.ReadRaw(endVertex);
    r.ReadRaw(angle);
    r.ReadRaw(linedefIndex);
    r.ReadRaw(side);
    r.ReadRaw(offset);
  }
};

struct BspNode
{
  BspNode() = default;
  BspNode(int16_t _x, int16_t _y, int16_t _dx, int16_t _dy);

  int16_t x = 0, y = 0, dx = 0, dy = 0;
  std::array<int16_t, 4> leftBoundingBox{ INT16_MAX, INT16_MIN, INT16_MIN, INT16_MAX };
  std::array<int16_t, 4> rightBoundingBox{ INT16_MAX, INT16_MIN, INT16_MIN, INT16_MAX };
  int16_t leftChild = -1, rightChild = -1;

  template<typename Writer> void serialize(Writer &w) const
  {
    w.WriteRaw(x);
    w.WriteRaw(y);
    w.WriteRaw(dx);
    w.WriteRaw(dy);

    w.WriteRaw(leftBoundingBox);
    w.WriteRaw(rightBoundingBox);

    w.WriteRaw(leftChild);
    w.WriteRaw(rightChild);
  }

  template<typename Reader> void deserialize(Reader &r)
  {
    r.ReadRaw(x);
    r.ReadRaw(y);
    r.ReadRaw(dx);
    r.ReadRaw(dy);

    r.ReadRaw(leftBoundingBox);
    r.ReadRaw(rightBoundingBox);

    r.ReadRaw(leftChild);
    r.ReadRaw(rightChild);
  }
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

  SubSector() = default;
  SubSector(const int16_t _segCount, const int16_t _firstSegIndex) : segCount(_segCount), firstSegIndex(_firstSegIndex)
  {}

  template<typename Writer> void serialize(Writer &w) const
  {
    w.WriteRaw(segCount);
    w.WriteRaw(firstSegIndex);
  }
  template<typename Reader> void deserialize(Reader &r)
  {
    r.ReadRaw(segCount);
    r.ReadRaw(firstSegIndex);
  }
};

struct SplitResult
{
  BspNode node;
  std::vector<Seg> front;
  std::vector<Seg> back;
};

struct BspLevel
{
  std::vector<Vertex> vertices;
  std::vector<LineDef> linedefs;

  std::vector<BspNode> nodes;
  std::vector<SubSector> subsectors;
  std::vector<Seg> segments;
};

class BSPBuilder
{
private:
  std::vector<Vertex> vertices;
  std::vector<LineDef> linedefs;

  std::vector<BspNode> nodes;
  std::vector<SubSector> subsectors;
  std::vector<Seg> segments;

private:
  SplitResult SplitBySplitter(std::vector<Seg> &segs, const Seg &splitter);
  void CreateSubsector(std::vector<Seg> &segs);
  [[nodiscard]] int16_t CreateLeafIndex() const;
  [[nodiscard]] uint32_t SelectSplittingLine(const std::vector<Seg> &segs) const;
  [[nodiscard]] uint32_t EvaluateSplitter(size_t splitterIndex, const std::vector<Seg> &segs) const;
  [[nodiscard]] SegmentPosition DetermineSegmentPosition(const Seg &splitter, const Seg &seg) const;
  [[nodiscard]] bool IsConvex(const std::vector<Seg> &segs) const;

  void AdjustBoundingBoxes(const std::vector<Seg> &segs, std::array<int16_t, 4> &boundingBox) const;
  void DrawSubsectors(const SdlWindow &sdlWindow) const;

  [[nodiscard]] size_t MaxDepth() const;
  [[nodiscard]] size_t MaxDepthRecursive(int16_t currentIndex) const;
  [[nodiscard]] static std::vector<std::string> FormatRows(const std::vector<std::vector<std::string>> &rows);
  [[nodiscard]] std::vector<std::vector<std::string>> BuildRows() const;
  void BuildRowsRecursive(int16_t index, size_t currentDepth, std::vector<std::vector<std::string>> &rows) const;

private:
  static void DrawBoundingBox(const SdlWindow &sdlWindow, const BspNode &root);
  static void DrawSplittingLine(const SdlWindow &sdlWindow, const BspNode &root);

public:
  explicit BSPBuilder(std::vector<Vertex> _vertices, std::vector<LineDef> _linedefs);

  void BuildBSPTree();
  int BuildBSPTree(std::vector<Seg> &segs);
  void PrintTree() const;
  void Visualize(const SdlWindow &sdlWindow, InputState &input) const;
  [[nodiscard]] std::unique_ptr<BspLevel> TakeConstructedLevel();
};
