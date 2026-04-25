#include "image_converter.h"
#include "colors_serializer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "defs.h"
#include "std_image.h"

#include <algorithm>
#include <iostream>

namespace image {

// helper: find the closest Doom palette color to a given RGB pixel
uint8_t FindClosestPaletteColor(const uint8_t r, const uint8_t g, const uint8_t b, const PaletteManager &palManager)
{
  int bestIndex = 0;
  int minDistance = std::numeric_limits<int>::max();

  // Loop through all 256 colors in the Doom palette
  for (int i = 0; i < 256; ++i) {
    const RGB color = palManager.GetColor(i);

    const int dr = r - color.r;
    const int dg = g - color.g;
    const int db = b - color.b;

    const int distanceSq = (dr * dr) + (dg * dg) + (db * db);

    if (distanceSq < minDistance) {
      minDistance = distanceSq;
      bestIndex = i;
    }
  }
  return static_cast<uint8_t>(bestIndex);
}


std::vector<uint32_t> ConvertFlatToRaw(const std::vector<uint8_t> &data, const PaletteManager &palManager)
{
  std::vector<uint32_t> res;
  res.reserve(data.size());

  for (const uint8_t index : data) {
    RGB color = palManager.GetColor(index);
    res.emplace_back(color.ToU32());
  }

  return std::move(res);
}
std::vector<uint32_t> ConvertWallToRaw(const uint16_t width,
                                       const uint16_t height,
                                       const std::vector<Post> &posts,
                                       const PaletteManager &palManager)
{
  std::vector<uint32_t> res(static_cast<size_t>(width) * height, 0x00000000);

  const size_t columnsToDecode = std::min(static_cast<size_t>(width), posts.size());
  for (size_t x = 0; x < columnsToDecode; ++x) {
    const Post &post = posts[x];
    if (post.pixels.empty()) {
      continue;
    }

    size_t y = std::min(static_cast<size_t>(post.topDelta), static_cast<size_t>(height));
    size_t pixelIndex = y * width + x;
    for (const uint8_t paletteIndex : post.pixels) {
      if (y >= height) {
        break;
      }

      const RGB color = palManager.GetColor(paletteIndex);
      res[pixelIndex] = color.ToU32();
      ++y;
      pixelIndex += width;
    }
  }

  return res;
}
}// namespace image
