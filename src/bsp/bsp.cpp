#include "bsp.h"

#include "../core/math_utils.h"
#include "config.h"
#include "renderer/game/renderer_helper.h"

#include <iostream>
#include <queue>

void BSPBuilder::AdjustBoundingBoxes(const std::vector<Seg> &segs, std::array<fixed_t, 4> &boundingBox) const
{
  for (const auto &seg : segs) {
    const Vertex start = vertices[seg.startVertex].fromCenterCoords(WINDOW_WIDTH, WINDOW_HEIGHT);
    const Vertex end = vertices[seg.endVertex].fromCenterCoords(WINDOW_WIDTH, WINDOW_HEIGHT);

    boundingBox[0] = std::min(boundingBox[0], static_cast<fixed_t>(start.y));
    boundingBox[0] = std::min(boundingBox[0], static_cast<fixed_t>(end.y));

    boundingBox[1] = std::max(boundingBox[1], static_cast<fixed_t>(start.x));
    boundingBox[1] = std::max(boundingBox[1], static_cast<fixed_t>(end.x));

    boundingBox[2] = std::max(boundingBox[2], static_cast<fixed_t>(start.y));
    boundingBox[2] = std::max(boundingBox[2], static_cast<fixed_t>(end.y));

    boundingBox[3] = std::min(boundingBox[3], static_cast<fixed_t>(start.x));
    boundingBox[3] = std::min(boundingBox[3], static_cast<fixed_t>(end.x));
  }
}

Node::Node(const fixed_t _x, const fixed_t _y, const fixed_t _dx, const fixed_t _dy) : x(_x), y(_y), dx(_dx), dy(_dy) {}

BSPBuilder::BSPBuilder(std::vector<Vertex> _vertices, std::vector<LineDef> _linedefs)
  : vertices(std::move(_vertices)), linedefs(std::move(_linedefs))
{
  subsectors.reserve(_linedefs.size() / 2);
}

void BSPBuilder::BuildBSPTree()
{
  std::vector<Seg> _segments;
  _segments.reserve(linedefs.size());

  for (size_t i = 0; i < linedefs.size(); i++) {
    auto &ld = linedefs[i];

    angle_t angle = GetLineAngle(ld);

    _segments.emplace_back(ld.start, ld.end, angle, static_cast<int16_t>(i), 0, 0);
    if (ld.backSidedef != -1) {
      // reverting the angle for the reverted side
      _segments.emplace_back(ld.end, ld.start, ANG270 - angle + ANG90, static_cast<int16_t>(i), 1, 0);
    }
  }

  BuildBSPTree(_segments);
}


SplitResult BSPBuilder::SplitBySplitter(std::vector<Seg> &segs, const Seg &splitter)
{
  SplitResult res{};

  const Vertex splitterStart = vertices[splitter.startVertex];
  const Vertex splitterDirection = vertices[splitter.endVertex] - splitterStart;

  for (auto &seg : segs) {
    const SegmentPosition pos = DetermineSegmentPosition(splitter, seg);

    if (pos == SegmentPosition::FRONT) {
      res.front.emplace_back(seg);
    } else if (pos == SegmentPosition::BACK) {
      res.back.emplace_back(seg);
    } else {
      // split the segment into two parts
      const Vertex segStart = vertices[seg.startVertex];
      const Vertex segDirection = vertices[seg.endVertex] - segStart;

      const Vertex diff = segDirection - splitterDirection;
      // check for seg == splitter
      if (diff.x == 0 && diff.y == 0)
        continue;

      const std::pair<double, double> sol =
        math_utils::FindLinesIntersection(splitterStart, splitterDirection, segStart, segDirection);
      const Vertex intersection = segStart + segDirection * sol.second;
      vertices.emplace_back(intersection);

      const int16_t newVertexId = static_cast<int16_t>(vertices.size() - 1);

      const LineDef segLd = linedefs[seg.linedefIndex];

      LineDef startLd{ seg.startVertex, newVertexId, segLd.type, segLd.frontSidedef, segLd.backSidedef };
      LineDef endLd{ newVertexId, seg.endVertex, segLd.type, segLd.frontSidedef, segLd.backSidedef };

      linedefs.emplace_back(startLd);
      linedefs.emplace_back(endLd);

      Seg newSeg{ seg.startVertex, newVertexId, seg.angle, static_cast<int16_t>(linedefs.size() - 2), seg.side, 0 };
      Seg newOtherSeg{ newVertexId, seg.endVertex, seg.angle, static_cast<int16_t>(linedefs.size() - 1), !seg.side, 0 };

      const SegmentPosition newSegPos = DetermineSegmentPosition(splitter, newSeg);

      if (newSegPos == SegmentPosition::FRONT) {
        res.front.emplace_back(newSeg);
        res.back.emplace_back(newOtherSeg);
      } else {
        res.front.emplace_back(newOtherSeg);
        res.back.emplace_back(newSeg);
      }
    }
  }

  res.node = Node{ splitterStart.x, splitterStart.y, splitterDirection.x, splitterDirection.y };

  return res;
}
void BSPBuilder::CreateSubsector(std::vector<Seg> &segs)
{
  const size_t firstIndex = segments.size();
  for (auto &seg : segs) {
    segments.emplace_back(seg);
  }

  subsectors.emplace_back(segs.size(), static_cast<int16_t>(firstIndex));
}

int16_t BSPBuilder::CreateLeafIndex() const
{
  return (1 << 15) | (static_cast<int16_t>(subsectors.size() - 1) & 0x7FFF);
}

int32_t BSPBuilder::BuildBSPTree(std::vector<Seg> &segs)
{
  // base cases
  if (segs.empty())
    return 1 << 31;
  else if (segs.size() <= 2 || IsConvex(segs)) {
    CreateSubsector(segs);
    return CreateLeafIndex();
  }

  // selecting a splitter line
  const uint32_t bestSplitterIndex = SelectSplittingLine(segs);
  // partitioning segments into two groups based on the splitter line
  SplitResult split = SplitBySplitter(segs, segs[bestSplitterIndex]);
  // recursively building the tree for each group a convex subsector is reached
  nodes.push_back(split.node);

  const int id = static_cast<int>(nodes.size() - 1);

  Node &currentNode = nodes[id];

  AdjustBoundingBoxes(split.front, currentNode.rightBoundingBox);
  AdjustBoundingBoxes(split.back, currentNode.leftBoundingBox);

  if (split.front.size() == segs.size() || split.back.size() == segs.size()) {
    // The chosen splitter didn't actually partition the space.
    // Remove the orphan node we already pushed (it has no valid children).
    nodes.pop_back();
    CreateSubsector(segs);
    return CreateLeafIndex();
  }

  const int rightId = BuildBSPTree(split.front);
  const int leftId = BuildBSPTree(split.back);

  nodes[id].leftChild = static_cast<int16_t>(leftId);
  nodes[id].rightChild = static_cast<int16_t>(rightId);

  return id;
}

uint32_t BSPBuilder::SelectSplittingLine(const std::vector<Seg> &segs) const
{
  fixed_t maxX = std::numeric_limits<int>::min();
  fixed_t maxY = std::numeric_limits<int>::min();
  fixed_t minX = std::numeric_limits<int>::max();
  fixed_t minY = std::numeric_limits<int>::max();

  // creating a bounding box for segments
  for (const auto &seg : segs) {
    fixed_t x1 = vertices[seg.startVertex].x;
    fixed_t y1 = vertices[seg.endVertex].y;

    maxX = std::max(maxX, x1);
    maxY = std::max(maxY, y1);
    minX = std::min(minX, x1);
    minY = std::min(minY, y1);
  }

  uint32_t bestSplitterIndex = 0;
  uint32_t bestScore = std::numeric_limits<uint32_t>::max();

  for (size_t i = 0; i < segs.size(); i++) {
    const uint32_t score = EvaluateSplitter(i, segs);

    if (score < bestScore) {
      bestScore = score;
      bestSplitterIndex = static_cast<uint32_t>(i);
    }
  }

  return bestSplitterIndex;
}

SegmentPosition BSPBuilder::DetermineSegmentPosition(const Seg &splitter, const Seg &seg) const
{
  if (seg == splitter)
    return SegmentPosition::FRONT;
  else if (seg.linedefIndex == splitter.linedefIndex && seg.startVertex == splitter.endVertex
           && seg.endVertex == splitter.startVertex) {
    // the opposite side of the portal
    return SegmentPosition::BACK;
  }

  const Vertex startSplitterVertex = vertices[splitter.startVertex];
  const Vertex startSegVertex = vertices[seg.startVertex];
  const Vertex endSegVertex = vertices[seg.endVertex];

  // cross product -> positive = left, negative = right, zero = collinear
  const Vertex splitterDirection = vertices[splitter.endVertex] - startSplitterVertex;

  const fixed_t startCross = splitterDirection.cross(startSegVertex - startSplitterVertex);
  const fixed_t endCross = splitterDirection.cross(endSegVertex - startSplitterVertex);

  if (startCross == 0 && endCross == 0) {
    // collinear
    const Vertex segDirection = endSegVertex - startSegVertex;
    if (splitterDirection * segDirection > 0) {
      // point in the same direction
      return SegmentPosition::FRONT;
    } else {
      // point in the opposite direction
      return SegmentPosition::BACK;
    }
  }

  // Check if segment spans the splitter (endpoints on opposite sides)
  if ((startCross > 0 && endCross < 0) || (startCross < 0 && endCross > 0)) {
    // Check if they actually intersect
    const Vertex segDirection = endSegVertex - startSegVertex;

    const std::pair<fixed_t, fixed_t> sol =
      math_utils::FindLinesIntersection(startSplitterVertex, splitterDirection, startSegVertex, segDirection);

    // If intersection parameter is strictly between 0 and 1 for segment to be splitted, the segments are spanning
    if (sol.second > 0 && (sol.second >> (FRAC_BITS - 1)) < 1) {
      return SegmentPosition::SPANNING;
    }

    // Intersection falls outside the segment — classify by whichever endpoint
    // is farther from the splitter line (larger absolute cross product).
    const fixed_t dominant = (std::abs(startCross) >= std::abs(endCross)) ? startCross : endCross;
    return (dominant <= 0) ? SegmentPosition::FRONT : SegmentPosition::BACK;
  }

  if (startCross <= 0 && endCross <= 0) {
    return SegmentPosition::FRONT;
  }

  return SegmentPosition::BACK;
}

// returns a score of the segment; the smaller, the better
uint32_t BSPBuilder::EvaluateSplitter(const size_t splitterIndex, const std::vector<Seg> &segs) const
{
  int left = 0, right = 0, spanning = 0;

  for (auto &seg : segs) {
    // 0 = front, 1 = back, 2 = spanning
    const SegmentPosition position = DetermineSegmentPosition(segs[splitterIndex], seg);

    if (position == SegmentPosition::FRONT)
      right++;
    else if (position == SegmentPosition::BACK)
      left++;
    else
      spanning++;
  }

  // If a splitter leaves one side completely empty, it does not partition the space.
  // It will immediately trigger the recursion safety valve and halt the builder.
  // Apply a massive penalty so it is never chosen over a line that actually splits space.
  if (left == 0 || right == 0) {
    return 999999;
  }
  return std::abs(left - right) + spanning * 8;
}

/*
 checks whether a set of segments forms a convex shape. A subsector is convex
 when no segment straddles (spans) any other segment's line — i.e. no further
 splitting is necessary.
*/
bool BSPBuilder::IsConvex(const std::vector<Seg> &segs) const
{
  for (auto &seg : segs) {
    for (auto &other : segs) {
      if (seg == other)
        continue;

      const SegmentPosition pos = DetermineSegmentPosition(seg, other);
      if (pos != SegmentPosition::FRONT)
        return false;
    }
  }

  return true;
}
angle_t BSPBuilder::GetLineAngle(const LineDef &ld) const
{
  const Vertex &start = vertices[ld.start];
  const Vertex &end = vertices[ld.end];

  return PointToAngle2(start.x, start.y, end.x, end.y);
}

/**
 * object becomes invalid to use after calling this method
 * @returns a unique pointer to a newly constructed BspLevel object,
 * all the parameters are being initilaized through std::move calls
 */
std::unique_ptr<BspLevel> BSPBuilder::TakeConstructedLevel()
{
  return std::make_unique<BspLevel>(
    std::move(vertices), std::move(linedefs), std::move(nodes), std::move(subsectors), std::move(segments));
}