// TODO: add colormap

#ifndef DOOMCLONE_COLORS_SERIALIZER_H
#define DOOMCLONE_COLORS_SERIALIZER_H

#include "serialization.h"


#include <array>
#include <cstdint>

// forward declarations
struct header;
class FileReader;

constexpr uint16_t PALETTE_SIZE = 256;
constexpr uint8_t NUMBER_OF_PALETTES = 14;

struct RGB
{
  uint8_t r, g, b;

  template<typename Writer>
  void serialize(Writer &w)
  {
    serialization::serialize(w, r);
    serialization::serialize(w, g);
    serialization::serialize(w, b);
  }

  [[nodiscard]] uint32_t ToU32() const { return (r << 24) | (g << 16) | (b << 8) | 255; }
};


class PaletteManager
{
public:
  PaletteManager(FileReader &fr, const header &hdr);
  explicit PaletteManager(FileReader &fr);

  void SetCurrentPalette(uint8_t index);
  [[nodiscard]] const RGB &GetColor(uint8_t index) const;

  template<class Writer>
  void Serialize(Writer &w);

private:
  void ReadPalette(FileReader &fr);

private:
  std::array<RGB, PALETTE_SIZE * NUMBER_OF_PALETTES> m_playpal{};
  RGB *m_activePal{};
};
#endif// DOOMCLONE_COLORS_SERIALIZER_H
