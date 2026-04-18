#include "image_converter.h"
#include "colors_serializer.h"

#define STB_IMAGE_IMPLEMENTATION
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

RawImage LoadRawImage(const std::filesystem::path &filepath, const PaletteManager &palManager)
{
  RawImage result;
  result.width = 0;
  result.height = 0;

  int width, height, channels;
  unsigned char *imgData = stbi_load(filepath.c_str(), &width, &height, &channels, 4);

  if (!imgData) {
    std::cerr << "Failed to load image: " << filepath << "\n";
    return result;
  }

  result.width = width;
  result.height = height;

  result.pixels.resize(width * height, -1);

  // Iterate through the image
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {

      // index for the raw bytes from stb_image
      const int byteIndex = (y * width + x) * 4;

      // index for RawImage vector
      const int pixelIndex = (y * width) + x;

      const uint8_t r = imgData[byteIndex + 0];
      const uint8_t g = imgData[byteIndex + 1];
      const uint8_t b = imgData[byteIndex + 2];
      const uint8_t a = imgData[byteIndex + 3];

      // if the alpha channel is less than 128, consider it fully transparent
      if (a < 128) {
        result.pixels[pixelIndex] = -1;
      } else {
        result.pixels[pixelIndex] = FindClosestPaletteColor(r, g, b, palManager);
      }
    }
  }

  // Free the raw memory allocated by stb_image
  stbi_image_free(imgData);

  return result;
}

std::vector<uint8_t> ConvertRawToFlat(const RawImage &img)
{
  std::vector<uint8_t> res;
  res.reserve(img.width * img.height);

  // just writing the colors row-wise
  for (const int16_t pixel : img.pixels) {
    if (pixel < 0)
      throw std::runtime_error("Unexpected negative value when convert into a flat texture.");

    res.push_back(static_cast<uint8_t>(pixel));
  }

  return std::move(res);
}

std::vector<uint8_t> ConvertRawToWall(const RawImage &img)
{
  // TODO: reserve required space for the res vector
  std::vector<uint8_t> res;

  // write header (width, height, left/top offsets)
  res.push_back(img.width & 0xFF);
  res.push_back((img.width >> 8) & 0xFF);

  res.push_back(img.height & 0xFF);
  res.push_back((img.height >> 8) & 0xFF);

  // putting offsets 0 for walls
  // left
  res.push_back(0);
  res.push_back(0);
  // top
  res.push_back(0);
  res.push_back(0);

  // write zeros for offsets (reserve 4 bytes for each offset)
  const uint32_t pointerSectionOffset = res.size();
  for (int x = 0; x < img.width; x++) {
    for (int i = 0; i < 4; i++) {
      res.push_back(0);
    }
  }

  // process image column-wise
  std::vector<uint32_t> columnOffsets;
  columnOffsets.reserve(img.width);

  for (int x = 0; x < img.width; x++) {
    // push offset
    columnOffsets.push_back(res.size());

    int y = 0;
    int index = (img.width * y) + x;
    std::vector<uint8_t> runPixels;
    runPixels.reserve(128);

    while (y < img.height) {
      runPixels.clear();
      // find solid part
      while (y < img.height && img.pixels[index] == -1) {
        y++;
        index += img.width;
      }

      if (y >= img.height)
        break;

      const int runStart = y;

      // 128 is the maximum size of the post
      while (y < img.height && img.pixels[index] != -1 && runPixels.size() <= 128) {
        runPixels.push_back(static_cast<uint8_t>(img.pixels[index]));
        y++;
        index += img.width;
      }

      // push row start
      res.push_back(runStart);
      // push pixels count
      res.push_back(runPixels.size());

      // write pixels
      for (uint8_t px : runPixels) {
        res.push_back(px);
      }
    }

    // column terminator
    res.push_back(0xFF);
  }

  // write column offsets
  for (int x = 0; x < img.width; x++) {
    const uint32_t offset = columnOffsets[x];
    uint32_t ptr = pointerSectionOffset + (x * 4);

    res[ptr++] = offset & 0xFF;
    res[ptr++] = (offset > 8) & 0xFF;
    res[ptr++] = (offset > 16) & 0xFF;
    res[ptr] = (offset > 24) & 0xFF;
  }

  return std::move(res);
}
}// namespace image
