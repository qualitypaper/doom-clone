#ifndef DOOMCLONE_WAD_PARSER_H
#define DOOMCLONE_WAD_PARSER_H

#include "serialization.h"

#include <array>
#include <cstdint>

struct header
{
  std::array<char8_t, 4> magicNumber;
  uint32_t numDirectories;
  uint32_t directoryOffset;

  template<typename Writer>
  void serialize(Writer &w) const
  {
    serialization::serialize(w, magicNumber);
    serialization::serialize(w, numDirectories);
    serialization::serialize(w, directoryOffset);
  }

  template<typename Reader>
  void deserialize(Reader &r)
  {
    serialization::deserialize(r, magicNumber);
    serialization::deserialize(r, numDirectories);
    serialization::deserialize(r, directoryOffset);
  }
};


struct directoryEntry
{
  uint32_t offset;
  uint32_t size;
  std::array<char8_t, 8> name;

  template<typename Writer>
  void serialize(Writer &w) const
  {
    serialization::serialize(w, offset);
    serialization::serialize(w, size);
    serialization::serialize(w, name);
  }

  template<typename Reader>
  void deserialize(Reader &r)
  {
    serialization::deserialize(r, offset);
    serialization::deserialize(r, size);
    serialization::deserialize(r, name);
  }
};

struct LumpData
{
  std::vector<uint8_t> rawData;
  directoryEntry entry{};
};


directoryEntry FindDirectory(FileReader &fr, const header &hdr, const std::array<char8_t, 8> &name);
std::vector<directoryEntry> ReadDirectory(FileReader &fr, const header &hdr);
void WriteLumpData(FileWriter &fw, LumpData &lumpData);
LumpData ReadLumpData(FileReader &fr, const directoryEntry &entry);
void WriteLumps(std::vector<LumpData> allLumps, header &_header);


std::array<char8_t, 8> MakeLevelName(uint16_t number);
std::array<char8_t, 8> MakeLumpName(std::string_view name);

#endif// DOOMCLONE_WAD_PARSER_H
