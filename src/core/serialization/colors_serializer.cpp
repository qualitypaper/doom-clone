#include "colors_serializer.h"

#include "serialization.h"
#include "wad_serializer.h"

#include <iostream>

constexpr PaletteManager::PaletteManager(FileReader &fr, const header &hdr)
{
  directoryEntry entry{};
  try {
    entry = FindDirectory(fr, hdr, MakeLumpName("PLAYPAL"));
  } catch (std::runtime_error &e) {
    std::cerr << e.what() << '\n';
  }


  fr.SetPos(entry.offset + sizeof(header));

  for (size_t k = 0; k < NUMBER_OF_PALETTES; k++) {
    for (size_t i = k * PALETTE_SIZE; i < (k + 1) * PALETTE_SIZE && fr.IsStreamGood(); i++) {
      uint8_t r, g, b;
      fr.ReadRaw(r);
      fr.ReadRaw(g);
      fr.ReadRaw(b);

      this->m_playpal[i] = { r, g, b };
    }
  }

  this->m_activePal = this->m_playpal.data();
}

void PaletteManager::SetCurrentPalette(const uint8_t index)
{
  if (index > NUMBER_OF_PALETTES) return;

  this->m_activePal = this->m_playpal.data() + PALETTE_SIZE * index;
}

const RGB &PaletteManager::GetColor(const uint8_t index) const {
  if (index > PALETTE_SIZE) return m_activePal[0];

  return m_activePal[index];
}