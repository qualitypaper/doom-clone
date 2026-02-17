#include "bsp.h"

#include "config.h"
#include "framebuffer.h"
#include "imgui_impl_sdl2.h"
#include "imgui_renderer.h"
#include "math_utils.h"

#include <queue>
#include <unordered_map>

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
    if (ld.backSidedef) { segments.emplace_back(ld.end, ld.start, 0, static_cast<int16_t>(i), 1); }
  }
}

void BSPBuilder::BuildBSPTree() { BuildBSPTree(segments); }

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

  nodes.reserve(level->linedefs.size());

  // selecting a splitter line
  const uint32_t bestSplitterIndex = SelectSplittingLine(segs);
  // partitioning segments into two groups based on the splitter line
  SplitResult split = SplitBySplitter(segs, segs[bestSplitterIndex]);
  // recursively building the tree for each group a convex subsector is reached
  nodes.push_back(split.node);

  const int id = static_cast<int>(nodes.size() - 1);

  const int leftId = BuildBSPTree(split.front);
  const int rightId = BuildBSPTree(split.back);

  nodes[id].leftChild = leftId;
  nodes[id].rightChild = rightId;

  return id;
}
void BSPBuilder::printTree() const
{
  for (auto &node : nodes) { std::cout << node << '\n'; }
}

void BSPBuilder::visualize() const
{
  SdlWindow sdlWindow("BSP Builder", config::EDITOR_WINDOW_WIDTH, config::EDITOR_WINDOW_HEIGHT);
  const imguirenderer::ImguiRenderer imguiRenderer(sdlWindow, *level);

  const time_t start = time(nullptr);
  uint64_t prev = SDL_GetPerformanceCounter();
  const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
  const double df = 1 / 144.0;
  bool running = true;

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) { ImGui_ImplSDL2_ProcessEvent(&event); }

    SDL_SetRenderDrawColor(sdlWindow.getRenderer(), 0, 0, 0, 255);
    imguiRenderer.render();

    const BspNode root = nodes[0];
    ImVec2 rootVertex = math_utils::fromCenterCoordinates(
      { static_cast<float>(root.x), static_cast<float>(root.y) }, sdlWindow.width, sdlWindow.height);

    SDL_SetRenderDrawColor(sdlWindow.getRenderer(), 255, 255, 0, 255);
    SDL_RenderDrawLine(sdlWindow.getRenderer(),
      rootVertex.x,
      rootVertex.y,
      static_cast<int>(static_cast<double>(root.dy) / static_cast<double>(root.dx + 0.001))
        * (sdlWindow.width - root.x),
      static_cast<int>(static_cast<double>(root.dx) / static_cast<double>(root.dy + 0.001))
        * (sdlWindow.height - root.y));

    SDL_RenderDrawLine(sdlWindow.getRenderer(),
      static_cast<int>(static_cast<double>(root.dy) / static_cast<double>(root.dx) * rootVertex.x),
      static_cast<int>(static_cast<double>(root.dx) / static_cast<double>(root.dy) * rootVertex.y),
      rootVertex.x,
      rootVertex.y);

    SDL_SetRenderDrawColor(sdlWindow.getRenderer(), 0, 255, 0, 255);
    for (const auto &subsector : subsectors) {
      for (int i = subsector.firstSegIndex; i < subsector.segCount + subsector.firstSegIndex; i++) {
        const auto &seg = newSegments[i];
        const auto &startVertex = level->vertices[seg.startVertex];
        const auto &endVertex = level->vertices[seg.endVertex];

        const auto mappedStart = math_utils::fromCenterCoordinates(
          { static_cast<float>(startVertex.x), static_cast<float>(startVertex.y) }, sdlWindow.width, sdlWindow.height);
        const auto mappedEnd = math_utils::fromCenterCoordinates(
          { static_cast<float>(endVertex.x), static_cast<float>(endVertex.y) }, sdlWindow.width, sdlWindow.height);

        // SDL_Rect rect{(int) mappedStart.x, (int) mappedStart.y+1, (int) (mappedEnd.x - mappedStart.x), (int)
        // (mappedEnd.y - mappedStart.y) + 1}; SDL_RenderFillRect(sdlWindow.getRenderer(), &rect);
        SDL_RenderDrawLine(sdlWindow.getRenderer(), mappedStart.x, mappedStart.y, mappedEnd.x, mappedEnd.y);
      }
    }

    SDL_RenderPresent(sdlWindow.getRenderer());

    const uint64_t now = SDL_GetPerformanceCounter();
    double frameTime = static_cast<double>(now - prev) / freq;
    prev = now;
    if (frameTime > 0.25) frameTime = 0.25;

    if (frameTime < df) { SDL_Delay(static_cast<uint64_t>((df - frameTime) * 1000.0)); }
    running = time(nullptr) - start < 60 * 1000;
  }
}

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

      const double k = math_utils::findLinesIntersection(splitterStart, splitterDirection, segStart, segDirection);
      const Vertex intersection = segStart + segDirection * k;
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
      if (segIndex == 10) { std::cout << ""; }
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

  // cross product -> positive = left, negative = right, zero = collinear
  const Vertex splitterDirection = level->vertices[splitter.endVertex] - startSplitterVertex;
  const Vertex segDirection = level->vertices[seg.endVertex] - startSegVertex;

  // check for line intersection
  if ((splitterDirection.x != 0 || splitterDirection.y != 0) && (segDirection.x != 0 || segDirection.y != 0)) {
    const double k =
      math_utils::findLinesIntersection(startSplitterVertex, splitterDirection, startSegVertex, segDirection);

    if (k > 0 && k < 1) { return SegmentPosition::SPANNING; }
  }

  const int32_t length = math_utils::crossProductLength(splitterDirection, startSegVertex);

  if (length <= 0) {
    // front (right), covers also lines that are collinear with the splitter
    return SegmentPosition::FRONT;
  } else {
    // back(left)
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

    if (position == SegmentPosition::FRONT)
      right++;
    else if (position == SegmentPosition::BACK)
      left++;
    else
      spanning++;
  }

  return std::abs(left - right) + spanning * 3;
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
