#include "bsp.h"

#include "math_utils.h"

#include <queue>
#include <unordered_map>

std::vector<BspNode> nodes;
std::vector<Seg> segments;
std::vector<SubSector> subsectors;


BSPBuilder::BSPBuilder(Level &_level) : level(_level)
{
  segments.reserve(level.linedefs.size());
  subsectors.reserve(level.linedefs.size() / 2);

  for (size_t i = 0; i < level.linedefs.size(); i++) {
    auto &ld = level.linedefs[i];
    segments.emplace_back(ld.start, ld.end, 0, static_cast<int16_t>(i), 0);
    if (ld.backSidedef) { segments.emplace_back(ld.end, ld.start, 0, static_cast<int16_t>(i), 1); }
  }
}

void BSPBuilder::BuildBSPTree(const std::vector<Seg> &segs)
{
  // base case
  if (segs.size() <= 2 || IsConvex(segs)) { return; }

  nodes.reserve(level.linedefs.size());

  // selecting a splitter line
  const uint32_t bestSplitterIndex = SelectSplittingLine(segs);
  // partitioning segments into two groups based on the splitter line
  const SplitResult split = SplitBySplitter(segs, segs[bestSplitterIndex]);
  // recursively building the tree for each group a convex subsector is reached
  nodes.push_back(split.node);

  BuildBSPTree(split.front);
  BuildBSPTree(split.back);
}

SplitResult BSPBuilder::SplitBySplitter(const std::vector<Seg> &segs, const Seg &splitter) {}

uint32_t BSPBuilder::SelectSplittingLine(const std::vector<Seg> &segs)
{
  int maxX = std::numeric_limits<int>::min();
  int maxY = std::numeric_limits<int>::min();
  int minX = std::numeric_limits<int>::max();
  int minY = std::numeric_limits<int>::max();

  // creating a bounding box for segments
  for (const auto &seg : segs) {
    int x1 = level.vertices[seg.startVertex].x;
    int y1 = level.vertices[seg.endVertex].y;

    maxX = std::max(maxX, x1);
    maxY = std::max(maxY, y1);
    minX = std::min(minX, x1);
    minY = std::min(minY, y1);
  }

  // pick center'ish lines as a potential splitters
  std::priority_queue<int, std::vector<int>, std::greater<>> minHeap;
  std::unordered_map<int, std::vector<size_t>> distanceToSegmentsMap;

  for (size_t i = 0; i < segs.size(); i++) {
    const int dx = std::min(
      std::abs(level.vertices[segs[i].startVertex].y - minY), std::abs(level.vertices[segs[i].startVertex].y - maxY));
    const int dy = std::min(
      std::abs(level.vertices[segs[i].startVertex].x - minX), std::abs(level.vertices[segs[i].startVertex].x - maxX));

    const int distanceSq = dx * dx + dy * dy;
    minHeap.push(distanceSq);
    distanceToSegmentsMap[distanceSq].push_back(i);
  }

  // pick top 10 lines as potential splitters and evaluate them
  size_t bestSplitterIndex = 0;
  uint32_t bestScore = std::numeric_limits<uint32_t>::max();

  for (size_t i = 0; i < segs.size() && !minHeap.empty(); i++) {
    const int distanceSq = minHeap.top();
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
  const Vertex startSplitterVertex = level.vertices[splitter.startVertex];
  const Vertex startSegVertex = level.vertices[seg.startVertex];

  // cross product -> positive = left, negative = right, zero = collinear
  const Vertex splitterDirection = level.vertices[splitter.endVertex] - startSplitterVertex;
  const Vertex segDirection = level.vertices[seg.endVertex] - startSegVertex;

  // check for line intersection
  if ((splitterDirection.x != 0 || splitterDirection.y != 0) && (segDirection.x != 0 || segDirection.y != 0)) {
    double k{};

    if (splitterDirection.x != 0) {
      double temp = static_cast<double>(splitterDirection.y) / static_cast<double>(splitterDirection.x);
      k = ((startSegVertex.y - startSplitterVertex.y - temp * (startSegVertex.x - startSplitterVertex.x)))
                 / (temp * segDirection.x - segDirection.y);

    } else if (splitterDirection.y != 0) {
      k = -(startSegVertex.x - startSplitterVertex.x) / static_cast<double>(startSegVertex.x);
    } else {
      throw std::runtime_error("Direction is a null vector.");
    }

    if (k > 0 && k < 1) {
      return SegmentPosition::SPANNING;
    } else if (k <= 0) {
      return SegmentPosition::FRONT;
    } else {
      return SegmentPosition::BACK;
    }
  }

  const int32_t length = math_utils::crossProductLength(splitterDirection, segDirection);

  if (length >= 0) {
    // front (left), covers also lines that are collinear with the splitter
    return SegmentPosition::FRONT;
  } else {
    // back(right)
    return SegmentPosition::BACK;
  }
}

// returns a score of the segment, the smaller the better
uint32_t BSPBuilder::EvaluateSplitter(const Seg &splitter) const
{
  int left = 0, right = 0, spanning = 0;

  for (auto &seg : segments) {
    // 0 = front, 1 = back, 2 = spanning
    const SegmentPosition position = DetermineSegmentPosition(splitter, seg);

    if (position == SegmentPosition::FRONT) left++;
    else if (position == SegmentPosition::BACK) right++;
    else if (position == SegmentPosition::SPANNING) spanning++;
  }

  return std::abs(left - right) + spanning * 8;
}

// checks whether a set of segments forms a convex shape
bool BSPBuilder::IsConvex(const std::vector<Seg> &segs) {}
