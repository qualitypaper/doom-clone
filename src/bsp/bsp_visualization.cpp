#include "bsp.h"
#include "imgui/imgui.h"
#include "math_utils.h"
#include "renderer/editor/editor_renderer.h"

void BSPBuilder::DrawBoundingBox(const SdlWindow &sdlWindow,
                                 const std::array<fixed_t, 4> boundingBox,
                                 const int r,
                                 const int g,
                                 const int b,
                                 const int a)
{
  const ImVec2 _min{ static_cast<float>(FixedToDouble(boundingBox[3])),
                     static_cast<float>(FixedToDouble(boundingBox[2])) };
  const ImVec2 _max{ static_cast<float>(FixedToDouble(boundingBox[1])),
                     static_cast<float>(FixedToDouble(boundingBox[0])) };

  const SDL_Rect _rect{ static_cast<int>(_min.x),
                        static_cast<int>(_max.y),
                        static_cast<int>(_max.x - _min.x),
                        static_cast<int>(_min.y - _max.y) };

  SDL_SetRenderDrawColor(sdlWindow.getRenderer(), r, g, b, a);
  SDL_RenderDrawRect(sdlWindow.getRenderer(), &_rect);
}

void BSPBuilder::DrawBoundingBoxes(const SdlWindow &sdlWindow, const Node &root)
{
  DrawBoundingBox(sdlWindow, root.leftBoundingBox, 255, 0, 255, 255);
  DrawBoundingBox(sdlWindow, root.rightBoundingBox, 0, 255, 255, 255);
}

void BSPBuilder::DrawSubsectors(const SdlWindow &sdlWindow, const Level &level)
{
  SDL_SetRenderDrawColor(sdlWindow.getRenderer(), 0, 255, 0, 255);

  for (const auto &subsector : level.subsectors) {
    for (int i = subsector.firstSegIndex; i < subsector.segCount + subsector.firstSegIndex; i++) {
      const auto &seg = level.segments[i];

      const ImVec2 mappedStart =
        seg.start->fromCenterCoords(sdlWindow.getRenderWidth(), sdlWindow.getRenderHeight()).toImVec();
      const ImVec2 mappedEnd =
        seg.end->fromCenterCoords(sdlWindow.getRenderWidth(), sdlWindow.getRenderHeight()).toImVec();

      SDL_RenderDrawLine(sdlWindow.getRenderer(), mappedStart.x, mappedStart.y, mappedEnd.x, mappedEnd.y);
    }
  }
}
void BSPBuilder::DrawSplittingLine(const SdlWindow &sdlWindow, const Node &root)
{
  static constexpr double lineLen = 2000.0;
  const double len = std::sqrt(FixedToDouble(FixedMul(root.dx, root.dx) + FixedMul(root.dy, root.dy)));
  const double dirX = FixedToDouble(root.dx) / len;
  const double dirY = FixedToDouble(root.dy) / len;

  const double rootX = FixedToDouble(root.x);
  const double rootY = FixedToDouble(root.y);

  const ImVec2 lineStart = math_utils::fromCenterCoordinates(
    ImVec2{ static_cast<float>(rootX - dirX * lineLen), static_cast<float>(rootY - dirY * lineLen) },
    sdlWindow.getRenderWidth(),
    sdlWindow.getRenderHeight());
  const ImVec2 lineEnd = math_utils::fromCenterCoordinates(
    ImVec2{ static_cast<float>(rootX + dirX * lineLen), static_cast<float>(rootY + dirY * lineLen) },
    sdlWindow.getRenderWidth(),
    sdlWindow.getRenderHeight());

  SDL_SetRenderDrawColor(sdlWindow.getRenderer(), 255, 255, 0, 255);
  SDL_RenderDrawLine(sdlWindow.getRenderer(),
                     static_cast<int>(lineStart.x),
                     static_cast<int>(lineStart.y),
                     static_cast<int>(lineEnd.x),
                     static_cast<int>(lineEnd.y));
}

void BSPBuilder::Visualize(const SdlWindow &sdlWindow, InputState &input, const Level &level)
{
  static size_t s_nodesIndex = 0;

  SDL_SetRenderDrawColor(sdlWindow.getRenderer(), 0, 0, 0, 255);
  SDL_RenderClear(sdlWindow.getRenderer());

  DrawSubsectors(sdlWindow, level);

  if (input.keys[SDL_SCANCODE_LEFT]) {
    if (s_nodesIndex > 0)
      s_nodesIndex--;
  } else if (input.keys[SDL_SCANCODE_RIGHT]) {
    if (!level.nodes.empty() && s_nodesIndex < level.nodes.size() - 1)
      s_nodesIndex++;
    input.keys[SDL_SCANCODE_RIGHT] = false;
  }

  if (level.nodes.empty()) {
    SDL_RenderPresent(sdlWindow.getRenderer());
    return;
  }

  const Node &root = level.nodes[s_nodesIndex];

  DrawSplittingLine(sdlWindow, root);
  DrawBoundingBoxes(sdlWindow, root);

  SDL_RenderPresent(sdlWindow.getRenderer());
}

size_t BSPBuilder::MaxDepth() const { return MaxDepthRecursive(0); }

size_t BSPBuilder::MaxDepthRecursive(const int16_t currentIndex) const
{
  if (currentIndex & 0x8000 || static_cast<size_t>(currentIndex) >= nodes.size() || currentIndex < 0)
    return 0;

  const size_t left = MaxDepthRecursive(nodes[currentIndex].leftChild);
  const size_t right = MaxDepthRecursive(nodes[currentIndex].rightChild);

  return 1 + std::max(left, right);
}

std::vector<std::vector<std::string>> BSPBuilder::BuildRows() const
{
  const size_t maxDepth = MaxDepth();
  std::vector<std::vector<std::string>> rows;
  if (maxDepth == 0) {
    return std::vector<std::vector<std::string>>();
  }

  rows.resize(maxDepth);

  rows[0].emplace_back("0");

  BuildRowsRecursive(0, 1, rows);

  return rows;
}

void BSPBuilder::BuildRowsRecursive(const int16_t index,
                                    const size_t currentDepth,
                                    std::vector<std::vector<std::string>> &rows) const
{
  if (currentDepth >= rows.size())
    return;
  if (index & 0x8000) {

    rows[currentDepth].push_back(std::to_string(index));
    return;
  }

  const int16_t left = nodes[index].leftChild;
  const int16_t right = nodes[index].rightChild;
  rows[currentDepth].push_back(std::to_string(left));
  rows[currentDepth].push_back(std::to_string(right));

  if (!(left & 0x8000)) {
    BuildRowsRecursive(left, currentDepth + 1, rows);
  }

  if (!(right & 0x8000)) {
    BuildRowsRecursive(right, currentDepth + 1, rows);
  }
}

std::vector<std::string> BSPBuilder::FormatRows(const std::vector<std::vector<std::string>> &rows)
{
  using s_t = std::string::size_type;

  // First find the maximum value string length and put it in cell_width
  s_t cell_width = 0;

  for (const auto &row_disp : rows) {
    for (const auto &cell : row_disp) {
      if (!cell.empty() && cell.length() > cell_width) {
        cell_width = cell.length();
      }
    }
  }

  // make sure the cell_width is an odd number
  if (cell_width % 2 == 0)
    ++cell_width;

  // allows leaf nodes to be connected when they are all with size of a single character
  if (cell_width < 3)
    cell_width = 3;

  // formatted_rows will hold the results
  std::vector<std::string> formatted_rows;

  // some of these counting variables are related,
  // so its should be possible to eliminate some of them.
  s_t row_count = rows.size();

  // this row's element count, a power of two
  s_t row_elem_count = 1 << (row_count - 1);

  // left_pad holds the number of space charactes at the beginning of the bottom row
  s_t left_pad = 0;

  // Work from the level of maximum depth, up to the root
  // ("formatted_rows" will need to be reversed when done)
  for (s_t r = 0; r < row_count; ++r) {
    const auto &cd_row = rows[row_count - r - 1];// r reverse-indexes the row
    // "space" will be the number of rows of slashes needed to get
    // from this row to the next.  It is also used to determine other
    // text offsets.
    s_t space = (s_t(1) << r) * (cell_width + 1) / 2 - 1;
    // "row" holds the line of text currently being assembled
    std::string row;
    // iterate over each element in this row
    for (s_t c = 0; c < cd_row.size(); ++c) {
      // add padding, more when this is not the leftmost element
      row += std::string(c ? left_pad * 2 + 1 : left_pad, ' ');

      if (!cd_row[c].empty()) {
        // This position corresponds to an existing Node
        const std::string &valstr = cd_row[c];
        // Try to pad the left and right sides of the value string
        // with the same number of spaces.  If padding requires an
        // odd number of spaces, right-sided children get the longer
        // padding on the right side, while left-sided children
        // get it on the left side.
        s_t long_padding = cell_width - valstr.length();
        s_t short_padding = long_padding / 2;
        long_padding -= short_padding;
        row += std::string(c % 2 ? short_padding : long_padding, ' ');
        row += valstr;
        row += std::string(c % 2 ? long_padding : short_padding, ' ');
      } else {
        // This position is empty, Nodeless...
        row += std::string(cell_width, ' ');
      }
    }
    // A row of spaced-apart value strings is ready, add it to the result vector
    formatted_rows.push_back(row);

    // The root has been added, so this loop is finished
    if (row_elem_count == 1)
      break;

    // Add rows of forward- and back- slash characters, spaced apart
    // to "connect" two rows' Node value strings.
    // The "space" variable counts the number of rows needed here.
    s_t left_space = space + 1;
    s_t right_space = space - 1;
    for (s_t sr = 0; sr < space; ++sr) {
      std::string row;

      for (s_t c = 0; c < cd_row.size(); ++c) {
        if (c % 2 == 0) {
          row += std::string(c ? left_space * 2 + 1 : left_space, ' ');
          row += !cd_row[c].empty() ? '/' : ' ';
          row += std::string(right_space + 1, ' ');
        } else {
          row += std::string(right_space, ' ');
          row += !cd_row[c].empty() ? '\\' : ' ';
        }
      }
      formatted_rows.push_back(row);
      ++left_space;
      --right_space;
    }
    left_pad += space + 1;
    row_elem_count /= 2;
  }

  // Reverse the result, placing the root node at the beginning (top)
  std::reverse(formatted_rows.begin(), formatted_rows.end());

  return formatted_rows;
}

void BSPBuilder::PrintTree() const
{
  const std::vector<std::vector<std::string>> rows = BuildRows();
  const std::vector<std::string> formattedRows = FormatRows(rows);

  std::cout << "BSP Tree: \n";
  for (const auto &row : formattedRows) {
    std::cout << ' ' << row << '\n';
  }
}
