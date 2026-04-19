#include "colors_serializer.h"

#include "serialization.h"
#include "wad_serializer.h"

#include <iostream>

PaletteManager::PaletteManager(FileReader &fr, const header &hdr)
{
  WadSerializer wadSerializer;
  wadSerializer.SetHeader(hdr);

  if (!wadSerializer.LoadDirectory(fr)) {
    throw std::runtime_error("Failed to read WAD directory while loading palette.");
  }

  const auto entry = wadSerializer.FindDirectoryEntry(WadSerializer::MakeLumpName("PLAYPAL"));
  if (!entry.has_value()) {
    throw std::runtime_error("PLAYPAL lump was not found.");
  }

  fr.SetPos(entry.value()->offset);

  ReadPalette(fr);

  this->m_activePal = this->m_playpal.data();
}

// expects a file at a position of exclusively binary data of the palette
PaletteManager::PaletteManager(FileReader &fr)
{
  ReadPalette(fr);
  this->m_activePal = this->m_playpal.data();
}

void PaletteManager::ReadPalette(FileReader &fr)
{
  for (size_t k = 0; k < NUMBER_OF_PALETTES; k++) {
    for (size_t i = k * PALETTE_SIZE; i < (k + 1) * PALETTE_SIZE && fr.IsStreamGood(); i++) {
      uint8_t r, g, b;
      fr.ReadRaw(r);
      fr.ReadRaw(g);
      fr.ReadRaw(b);

      this->m_playpal[i] = { r, g, b };
    }
  }
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

template<typename Writer>
void PaletteManager::Serialize(Writer &w)
{
  for (RGB rgb : m_playpal) {
    rgb.serialize(w);
  }
}