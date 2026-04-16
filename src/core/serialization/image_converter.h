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

class Image
{
public:
  Image(const std::filesystem::path &path, const PaletteManager &palManager);

private:
  int width, height;
  std::vector<uint8_t> m_pixels;

private:
  void ConvertRawToWad(const RawImage &img);
  [[nodiscard]] static uint8_t FindClosestPaletteColor(uint8_t r, uint8_t g, uint8_t b, const PaletteManager &palManager);
  [[nodiscard]] static RawImage LoadRawImage(const std::filesystem::path &path, const PaletteManager &palManager);
};


#endif// DOOMCLONE_IMAGE_CONVERTER_H
