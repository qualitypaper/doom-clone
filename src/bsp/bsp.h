#pragma once
#include "core/serialization/serialization.h"
#include "defs.h"

#include <array>
#include <fstream>
#include <memory>

class SdlWindow;
struct InputState;
struct Level;

enum class SegmentPosition { FRONT, BACK, SPANNING };

inline std::ostream &operator<<(std::ostream &outs, const Node &node)
{
  return outs << "BSPNode{ Coords: (" << node.x << ", " << node.y << ", " << node.dx << ", " << node.dy
              << "), Children: (" << node.leftChild << "," << node.rightChild << ")";
}

struct SplitResult
{
  Node node;
  std::vector<Seg> front;
  std::vector<Seg> back;
};

struct BspLevel
{
  std::vector<Vertex> vertices;
  std::vector<LineDef> linedefs;

  std::vector<Node> nodes;
  std::vector<SubSector> subsectors;
  std::vector<Seg> segments;
};

class BSPBuilder
{
private:
  std::vector<Vertex> vertices;
  std::vector<LineDef> linedefs;

  std::vector<Node> nodes;
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
  [[nodiscard]] angle_t GetLineAngle(const LineDef &ld) const;

  void AdjustBoundingBoxes(const std::vector<Seg> &segs, std::array<fixed_t, 4> &boundingBox) const;

  [[nodiscard]] size_t MaxDepth() const;
  [[nodiscard]] size_t MaxDepthRecursive(int16_t currentIndex) const;
  [[nodiscard]] static std::vector<std::string> FormatRows(const std::vector<std::vector<std::string>> &rows);
  [[nodiscard]] std::vector<std::vector<std::string>> BuildRows() const;
  void BuildRowsRecursive(int16_t index, size_t currentDepth, std::vector<std::vector<std::string>> &rows) const;

private:
  static void DrawSubsectors(const SdlWindow &sdlWindow, const Level &level);
  static void DrawBoundingBox(const SdlWindow &sdlWindow, std::array<fixed_t, 4> boundingBox, int r, int g, int b, int a);
  static void DrawBoundingBoxes(const SdlWindow &sdlWindow, const Node &root);
  static void DrawSplittingLine(const SdlWindow &sdlWindow, const Node &root);

public:
  explicit BSPBuilder(std::vector<Vertex> _vertices, std::vector<LineDef> _linedefs);

  void BuildBSPTree();
  int BuildBSPTree(std::vector<Seg> &segs);
  void PrintTree() const;
  [[nodiscard]] std::unique_ptr<BspLevel> TakeConstructedLevel();

public:
  static void Visualize(const SdlWindow &sdlWindow, InputState &input, const Level &level);
};
