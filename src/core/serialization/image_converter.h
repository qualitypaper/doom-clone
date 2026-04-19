#ifndef DOOMCLONE_IMAGE_CONVERTER_H
#define DOOMCLONE_IMAGE_CONVERTER_H

#include "colors_serializer.h"

#include <cstdint>
#include <filesystem>
#include <vector>

struct RawImage
{
  int width;
  int height;
  std::vector<int16_t> pixels;
};

namespace image {
[[nodiscard]] std::vector<uint8_t> ConvertRawToFlat(const RawImage &img);
[[nodiscard]] std::vector<uint8_t> ConvertRawToWall(const RawImage &img);
[[nodiscard]] uint8_t FindClosestPaletteColor(uint8_t r, uint8_t g, uint8_t b, const PaletteManager &palManager);
[[nodiscard]] RawImage LoadRawImage(const std::filesystem::path &filepath, const PaletteManager &palManager);
[[nodiscard]] std::vector<uint32_t> ConvertFlatToRaw(const std::vector<uint8_t> &data, const PaletteManager &palManager);
}// namespace image


#endif// DOOMCLONE_IMAGE_CONVERTER_H
