#ifndef DOOMCLONE_TEXTURE_SERIALIZER_H
#define DOOMCLONE_TEXTURE_SERIALIZER_H

#include "config.h"
#include "wad_serializer.h"


#include <algorithm>
#include <cstdint>
#include <vector>

inline void WriteFlat(std::vector<uint8_t> img, const std::array<char8_t, 8> &name)
{
  WadSerializer wadSerializer(SAVED_LEVEL_PATH);
  if (!wadSerializer.Load(true)) {
    return;
  }

  std::vector<LumpData> lumps = wadSerializer.GetLoadedLumps();
  const auto it =
    std::find_if(lumps.begin(), lumps.end(), [&](const LumpData &lump) { return lump.entry.name == name; });

  LumpData flatLump{};
  flatLump.entry.name = name;
  flatLump.rawData = std::move(img);

  if (it == lumps.end()) {
    lumps.push_back(std::move(flatLump));
  } else {
    *it = std::move(flatLump);
  }

  header hdr = wadSerializer.GetHeader();
  wadSerializer.SetHeader(hdr);
  wadSerializer.SetLumps(std::move(lumps));
  wadSerializer.Write();
}

#endif// DOOMCLONE_TEXTURE_SERIALIZER_H
