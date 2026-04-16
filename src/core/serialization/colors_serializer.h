//
// Created by qualitypaper on 4/16/26.
//

#ifndef DOOMCLONE_COLORS_SERIALIZER_H
#define DOOMCLONE_COLORS_SERIALIZER_H

#include <array>
#include <cstdint>

// forward declarations
struct header;
class FileReader;

constexpr uint8_t PALETTE_SIZE = 256;
constexpr uint8_t NUMBER_OF_PALETTES = 14;

struct RGB
{
  uint8_t r, g, b;
};


class PaletteManager
{
public:
  constexpr PaletteManager(FileReader &fr, const header &hdr);

  void SetCurrentPalette(uint8_t index);
  [[nodiscard]] const RGB &GetColor(uint8_t index) const;

private:
  std::array<RGB, PALETTE_SIZE * NUMBER_OF_PALETTES> m_playpal{};
  RGB *m_activePal{};
};
#endif// DOOMCLONE_COLORS_SERIALIZER_H
