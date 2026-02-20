#include "bsp.h"

#include "config.h"
#include "math_utils.h"

#include <format>
#include <queue>
#include <unordered_map>

void BSPBuilder::AdjustBoundingBoxes(const std::vector<Seg> &segs, std::array<int16_t, 4> &boundingBox) const
{
  for (const auto &seg : segs) {
    const Vertex start = math_utils::fromCenterCoordinates(
      level->vertices[seg.startVertex], config::EDITOR_WINDOW_WIDTH, config::EDITOR_WINDOW_HEIGHT);
    const Vertex end = math_utils::fromCenterCoordinates(
      level->vertices[seg.endVertex], config::EDITOR_WINDOW_WIDTH, config::EDITOR_WINDOW_HEIGHT);


    boundingBox[0] = std::min(boundingBox[0], static_cast<int16_t>(start.y));
    boundingBox[0] = std::min(boundingBox[0], static_cast<int16_t>(end.y));

    boundingBox[1] = std::max(boundingBox[1], static_cast<int16_t>(start.x));
    boundingBox[1] = std::max(boundingBox[1], static_cast<int16_t>(end.x));

    boundingBox[2] = std::max(boundingBox[2], static_cast<int16_t>(start.y));
    boundingBox[2] = std::max(boundingBox[2], static_cast<int16_t>(end.y));

    boundingBox[3] = std::min(boundingBox[3], static_cast<int16_t>(start.x));
    boundingBox[3] = std::min(boundingBox[3], static_cast<int16_t>(end.x));
  }
}

BspNode::BspNode(const int16_t _x, const int16_t _y, const int16_t _dx, const int16_t _dy)
  : x(_x), y(_y), dx(_dx), dy(_dy)
{}

BSPBuilder::BSPBuilder(const Level &_level) : level(std::make_unique<Level>(_level))
{
  segments.reserve(level->linedefs.size());
  subsectors.reserve(level->linedefs.size() / 2);

  for (size_t i = 0; i < level->linedefs.size(); i++) {
    auto &ld = level->linedefs[i];
    segments.emplace_back(ld.start, ld.end, 0, static_cast<int16_t>(i), 0);
    if (ld.backSidedef != -1) { segments.emplace_back(ld.end, ld.start, 0, static_cast<int16_t>(i), 1); }
  }
}

void BSPBuilder::BuildBSPTree() { BuildBSPTree(segments); }


SplitResult BSPBuilder::SplitBySplitter(std::vector<Seg> &segs, const Seg &splitter) const
{
  SplitResult res{};

  const Vertex splitterStart = level->vertices[splitter.startVertex];
  const Vertex splitterDirection = level->vertices[splitter.endVertex] - splitterStart;

  for (auto &seg : segs) {
    if (seg.endVertex == splitter.endVertex && seg.startVertex == splitter.startVertex) continue;

    const SegmentPosition pos = DetermineSegmentPosition(splitter, seg);

    if (pos == SegmentPosition::FRONT) {
      seg.side = 0;
      res.front.emplace_back(seg);
    } else if (pos == SegmentPosition::BACK) {
      seg.side = 1;
      res.back.emplace_back(seg);
    } else {
      // split the segment into two parts
      const Vertex segStart = level->vertices[seg.startVertex];
      const Vertex segDirection = level->vertices[seg.endVertex] - segStart;

      const Vertex diff = segDirection - splitterDirection;
      // check for seg == splitter
      if (diff.x == 0 && diff.y == 0) continue;

      const std::pair<double, double> sol =
        math_utils::findLinesIntersection(splitterStart, splitterDirection, segStart, segDirection);
      const Vertex intersection = segStart + segDirection * sol.second;
      level->vertices.emplace_back(intersection);
      const int16_t newVertexId = static_cast<int16_t>(level->vertices.size() - 1);

      const LineDef segLd = level->linedefs[seg.linedefIndex];
      LineDef startLd{ segLd.start, newVertexId, segLd.type, segLd.frontSidedef, segLd.backSidedef };
      LineDef endLd{ newVertexId, segLd.end, segLd.type, segLd.frontSidedef, segLd.backSidedef };

      level->linedefs.emplace_back(startLd);
      level->linedefs.emplace_back(endLd);

      Seg newSeg{ segLd.start, newVertexId, 0, static_cast<int16_t>(level->linedefs.size() - 2), 0, 0 };
      Seg newOtherSeg{ newVertexId, segLd.end, 0, static_cast<int16_t>(level->linedefs.size() - 1), 0, 0 };

      const SegmentPosition newSegPos = DetermineSegmentPosition(splitter, newSeg);

      if (newSegPos == SegmentPosition::FRONT) {
        newSeg.side = 0;
        newOtherSeg.side = 1;

        res.front.emplace_back(newSeg);
        res.back.emplace_back(newOtherSeg);
      } else {
        newSeg.side = 1;
        newOtherSeg.side = 0;

        res.front.emplace_back(newOtherSeg);
        res.back.emplace_back(newSeg);
      }
    }
  }

  res.node = { static_cast<int16_t>(splitterStart.x),
    static_cast<int16_t>(splitterStart.y),
    static_cast<int16_t>(splitterDirection.x),
    static_cast<int16_t>(splitterDirection.y) };

  return res;
}
int BSPBuilder::BuildBSPTree(std::vector<Seg> &segs)
{
  // base case
  if (segs.empty())
    return -1;
  else if (segs.size() <= 2 || IsConvex(segs)) {
    const size_t firstIndex = newSegments.size();
    for (auto &seg : segs) { newSegments.emplace_back(seg); }

    subsectors.emplace_back(segs.size(), static_cast<int16_t>(firstIndex));
    return -1;
  }

  // selecting a splitter line
  const uint32_t bestSplitterIndex = SelectSplittingLine(segs);
  // partitioning segments into two groups based on the splitter line
  SplitResult split = SplitBySplitter(segs, segs[bestSplitterIndex]);
  // recursively building the tree for each group a convex subsector is reached
  nodes.push_back(split.node);

  const int id = static_cast<int>(nodes.size() - 1);

  BspNode &currentNode = nodes[id];

  AdjustBoundingBoxes(split.front, currentNode.rightBoundingBox);
  AdjustBoundingBoxes(split.back, currentNode.leftBoundingBox);

  const int rightId = BuildBSPTree(split.front);
  const int leftId = BuildBSPTree(split.back);

  nodes[id].leftChild = static_cast<int16_t>(leftId);
  nodes[id].rightChild = static_cast<int16_t>(rightId);

  return id;
}

uint32_t BSPBuilder::SelectSplittingLine(const std::vector<Seg> &segs) const
{
  int maxX = std::numeric_limits<int>::min();
  int maxY = std::numeric_limits<int>::min();
  int minX = std::numeric_limits<int>::max();
  int minY = std::numeric_limits<int>::max();

  // creating a bounding box for segments
  for (const auto &seg : segs) {
    int x1 = level->vertices[seg.startVertex].x;
    int y1 = level->vertices[seg.endVertex].y;

    maxX = std::max(maxX, x1);
    maxY = std::max(maxY, y1);
    minX = std::min(minX, x1);
    minY = std::min(minY, y1);
  }

  // pick center'ish lines as a potential splitters
  std::priority_queue<int, std::vector<int>, std::greater<>> minHeap;
  std::unordered_map<int, std::vector<size_t>> distanceToSegmentsMap;

  for (size_t i = 0; i < segs.size(); i++) {
    const int dy = std::min(
      std::abs(level->vertices[segs[i].startVertex].y - minY), std::abs(level->vertices[segs[i].startVertex].y - maxY));
    const int dx = std::min(
      std::abs(level->vertices[segs[i].startVertex].x - minX), std::abs(level->vertices[segs[i].startVertex].x - maxX));

    const int distanceSq = dx * dx + dy * dy;
    if (!distanceToSegmentsMap.contains(distanceSq)) { minHeap.push(distanceSq); }
    distanceToSegmentsMap[distanceSq].push_back(i);
  }
  size_t bestSplitterIndex = 0;
  uint32_t bestScore = std::numeric_limits<uint32_t>::max();

  while (!minHeap.empty()) {
    int distanceSq = minHeap.top();
    minHeap.pop();

    for (const size_t segIndex : distanceToSegmentsMap[distanceSq]) {
      const uint32_t score = EvaluateSplitter(segs[segIndex]);

      if (score < bestScore) {
        bestScore = score;
        bestSplitterIndex = segIndex;
      }
    }
  }

  return bestSplitterIndex;
}

SegmentPosition BSPBuilder::DetermineSegmentPosition(const Seg &splitter, const Seg &seg) const
{
  const Vertex startSplitterVertex = level->vertices[splitter.startVertex];
  const Vertex startSegVertex = level->vertices[seg.startVertex];
  const Vertex endSegVertex = level->vertices[seg.endVertex];

  // cross product -> positive = left, negative = right, zero = collinear
  const Vertex splitterDirection = level->vertices[splitter.endVertex] - startSplitterVertex;

  const int32_t startCross =
    math_utils::crossProductLengthNDir(splitterDirection, startSegVertex - startSplitterVertex);
  const int32_t endCross = math_utils::crossProductLengthNDir(splitterDirection, endSegVertex - startSplitterVertex);

  if (startCross == 0 && endCross == 0) {
    // collinear
    return SegmentPosition::FRONT;
  }

  // Check if segment spans the splitter (endpoints on opposite sides)
  if ((startCross > 0 && endCross < 0) || (startCross < 0 && endCross > 0)) {
    // Check if they actually intersect
    const Vertex segDirection = endSegVertex - startSegVertex;

    const std::pair<double, double> sol =
      math_utils::findLinesIntersection(startSplitterVertex, splitterDirection, startSegVertex, segDirection);
    // If intersection parameter is strictly between 0 and 1 for segment to be splitted, the segments are spanning
    if (sol.second > 0 && sol.second < 1) { return SegmentPosition::SPANNING; }
  }

  if (startCross <= 0 && endCross <= 0) {
    return SegmentPosition::FRONT;
  } else if (startCross >= 0 && endCross >= 0) {
    return SegmentPosition::BACK;
  } else {
    return SegmentPosition::SPANNING;
  }
}

// returns a score of the segment; the smaller, the better
uint32_t BSPBuilder::EvaluateSplitter(const Seg &splitter) const
{
  if (splitter.linedefIndex == 1) { std::cout << ""; }
  int left = 0, right = 0, spanning = 0;

  for (auto &seg : segments) {
    // 0 = front, 1 = back, 2 = spanning
    if (seg.linedefIndex == splitter.linedefIndex) continue;

    const SegmentPosition position = DetermineSegmentPosition(splitter, seg);

    if (position == SegmentPosition::FRONT)
      right++;
    else if (position == SegmentPosition::BACK)
      left++;
    else
      spanning++;
  }

  return std::abs(left - right) + spanning * 8;
}

/*
 checks whether a set of segments forms a convex shape. A subsector (list of segments) forms a convex region,
 when any line drawn through the subsector crosses at most 2 other lines
*/
bool BSPBuilder::IsConvex(const std::vector<Seg> &segs) const
{
  for (auto &seg : segs) {
    for (auto &other : segs) {
      const SegmentPosition pos = DetermineSegmentPosition(seg, other);
      if (pos != SegmentPosition::FRONT) return false;
    }
  }

  return true;
}

size_t BSPBuilder::MaxDepth() const { return MaxDepthRecursive(0); }

size_t BSPBuilder::MaxDepthRecursive(const int16_t currentIndex) const
{
  const size_t left = nodes[currentIndex].leftChild == -1 ? 0 : MaxDepthRecursive(nodes[currentIndex].leftChild);
  const size_t right = nodes[currentIndex].rightChild == -1 ? 0 : MaxDepthRecursive(nodes[currentIndex].rightChild);

  return 1 + left + right;
}

std::vector<std::vector<int16_t>> BSPBuilder::BuildRows() const
{
  const size_t maxDepth = MaxDepth();
  std::vector<std::vector<int16_t>> rows;

  rows.resize(maxDepth);

  rows[0].push_back(0);

  BuildRowsRecursive(0, 1, rows);

  return rows;
}

void BSPBuilder::BuildRowsRecursive(const int16_t index,
  const size_t currentDepth,
  std::vector<std::vector<int16_t>> &rows) const
{
  if (currentDepth >= rows.size()) return;

  const int16_t left = nodes[index].leftChild;
  const int16_t right = nodes[index].rightChild;
  rows[currentDepth].push_back(left);
  rows[currentDepth].push_back(right);

  if (left != -1) { BuildRowsRecursive(left, currentDepth + 1, rows); }

  if (right != -1) { BuildRowsRecursive(right, currentDepth + 1, rows); }
}

std::string BSPBuilder::FormatRows(const std::vector<std::vector<int16_t>> &rows)
{
  std::stringstream ss;

  const size_t depth = rows.size();
  const size_t width =
    std::max_element(rows.begin(), rows.end(), [](const std::vector<int16_t> &v1, const std::vector<int16_t> &v2) {
      return v1.size() < v2.size();
    })->size();

  for (size_t currDepth = 0; currDepth < depth; currDepth++) {
    const size_t n = rows[currDepth].size();

    for (const int16_t index : rows[currDepth]) {
      if (static_cast<int64_t>(width / n) > 0) { ss << std::string(width / n, ' '); }
      ss << index;
    }

    ss << '\n';

    for (size_t i = 0; i < n; i++) {
      if (rows[currDepth][i] != -1) {
        if (static_cast<int64_t>(width / n) - 1 > 0) { ss << std::string(width / n - 1, ' '); }
        ss << '/';
        ss << '\\';
      }
    }

    ss << '\n';
  }

  return ss.str();
}

void BSPBuilder::PrintTree() const
{
  const std::vector<std::vector<int16_t>> rows = BuildRows();
  const std::string res = FormatRows(rows);

  std::cout << res << '\n';
}
