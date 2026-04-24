#ifndef DOOMCLONE_IMAGE_CONVERTER_H
#define DOOMCLONE_IMAGE_CONVERTER_H

#include "colors_serializer.h"
#include "defs.h"
#include "std_image.h"
#include "utils.h"

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

template<std::integral T>
struct RawImage
{
  int width;
  int height;
  // index into the palette, -1 means transparent
  std::vector<T> pixels;
};

namespace image {

[[nodiscard]] uint8_t FindClosestPaletteColor(uint8_t r, uint8_t g, uint8_t b, const PaletteManager &palManager);
[[nodiscard]] std::vector<uint32_t> ConvertFlatToRaw(const std::vector<uint8_t> &data,
                                                     const PaletteManager &palManager);

template<typename T>
void ConvertRawColumnIntoWall(std::vector<Post> &posts, const RawImage<T> &img, int col);

template<typename T>
[[nodiscard]] std::vector<uint8_t> ConvertRawToFlat(const RawImage<T> &img);

template<typename T>
[[nodiscard]] Patch ConvertRawToWall(const RawImage<T> &img);

template<typename T>
[[nodiscard]] RawImage<T> LoadRawImage(const std::filesystem::path &filepath, const PaletteManager &palManager);

template<typename T>
RawImage<T> LoadRawImage(const std::filesystem::path &filepath, const PaletteManager &palManager)
{
  RawImage<T> result;
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

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int byteIndex = (y * width + x) * 4;
      const int pixelIndex = (y * width) + x;

      const uint8_t r = imgData[byteIndex + 0];
      const uint8_t g = imgData[byteIndex + 1];
      const uint8_t b = imgData[byteIndex + 2];
      const uint8_t a = imgData[byteIndex + 3];

      if (a < 128) {
        result.pixels[pixelIndex] = -1;
      } else {
        result.pixels[pixelIndex] = FindClosestPaletteColor(r, g, b, palManager);
      }
    }
  }

  stbi_image_free(imgData);
  return result;
}

template<typename T>
std::vector<uint8_t> ConvertRawToFlat(const RawImage<T> &img)
{
  std::vector<uint8_t> res;
  res.reserve(img.width * img.height);

  for (const int16_t pixel : img.pixels) {
    if (pixel < 0) {
      throw std::runtime_error("Unexpected negative value when convert into a flat texture.");
    }

    res.push_back(static_cast<uint8_t>(pixel));
  }

  return res;
}

template<typename T>
void ConvertRawColumnIntoWall(std::vector<Post> &posts, const RawImage<T> &img, const int col)
{
  int index = col;
  int y = 0;

  while (y < img.height) {
    std::vector<uint8_t> runPixels;
    runPixels.reserve(128);

    while (IsSignedIntPtr(img.pixels.data()) && y < img.height && img.pixels[index] == -1) {
      y++;
      index += img.width;
    }

    if (y >= img.height) {
      break;
    }

    const uint8_t runStart = static_cast<uint8_t>(y);
    while (y < img.height && img.pixels[index] != -1 && runPixels.size() <= 128) {
      runPixels.push_back(static_cast<uint8_t>(img.pixels[index]));
      y++;
      index += img.width;
    }

    runPixels.shrink_to_fit();
    posts.emplace_back(runStart, std::move(runPixels));
  }
}

template<typename T>
Patch ConvertRawToWall(const RawImage<T> &img)
{
  Patch patch{ .width = static_cast<uint16_t>(img.width),
               .height = static_cast<uint16_t>(img.height),
               .leftOffset = 0,
               .topOffset = 0,
               .posts = {} };
  patch.posts.reserve(patch.width);

  for (int x = 0; x < img.width; x++) {
    ConvertRawColumnIntoWall(patch.posts, img, x);
  }

  return patch;
}
}// namespace image


#endif// DOOMCLONE_IMAGE_CONVERTER_H
