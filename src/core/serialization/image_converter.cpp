#include "image_converter.h"
#include "colors_serializer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "defs.h"
#include "std_image.h"

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
}// namespace image
