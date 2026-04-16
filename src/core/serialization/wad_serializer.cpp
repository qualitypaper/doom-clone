#include "wad_serializer.h"

#include "serialization.h"
#include "config.h"

LumpData ReadLumpData(FileReader &fr, const directoryEntry &entry)
{
  LumpData lumpData;
  lumpData.entry = entry;

  fr.SetPos(entry.offset);

  lumpData.rawData.resize(entry.size);
  fr.ReadData(reinterpret_cast<char *>(lumpData.rawData.data()), entry.size);

  return lumpData;
}

void WriteLumpData(FileWriter &fw, LumpData &lumpData)
{
  lumpData.entry.offset = static_cast<uint32_t>(fw.Cursor());
  lumpData.entry.size = static_cast<uint32_t>(lumpData.rawData.size());

  fw.WriteData(reinterpret_cast<const char *>(lumpData.rawData.data()), lumpData.rawData.size());
}


std::vector<directoryEntry> ReadDirectory(FileReader &fr, const header &hdr)
{
  std::vector<directoryEntry> entries;
  entries.reserve(hdr.numDirectories);

  fr.SetPos(sizeof(header) + hdr.directoryOffset);

  for (uint32_t i = 0; i < hdr.numDirectories && fr.IsStreamGood(); ++i) {
    directoryEntry entry{};
    fr.ReadRaw(entry);
    entries.push_back(entry);
  }

  return entries;
}

directoryEntry FindDirectory(FileReader &fr, const header &hdr, const std::array<char8_t, 8> &name)
{
  const size_t initialPosition = fr.Cursor();

  fr.SetPos(hdr.directoryOffset + sizeof(hdr));

  for (int i = 0; i < hdr.numDirectories && fr.IsStreamGood(); i++) {
    directoryEntry entry{};
    fr.ReadRaw(entry);

    if (entry.name == name) {
      fr.SetPos(initialPosition);
      return entry;
    }
  }

  throw std::runtime_error(std::string("Directory with name ") + (char*) name.data() + "couldn't be found");
}

size_t WriteDirectoryAndGetOffset(FileWriter &fw, const std::vector<directoryEntry> &entries)
{
  const size_t dirOffset = fw.Cursor();

  for (const auto &entry : entries) {
    fw.WriteRaw(entry);
  }

  return dirOffset;
}

void WriteLumps(std::vector<LumpData> allLumps, header &_header)
{
  FileWriter fw(SAVED_LEVEL_PATH, std::ios::binary | std::ios::trunc);

  if (!fw.IsStreamGood()) {
    throw std::runtime_error("Failed to open file for saving.\n");
  }

  fw.WriteRaw(_header);

  std::vector<const directoryEntry *> finalEntries;
  for (auto &lumpData : allLumps) {
    WriteLumpData(fw, lumpData);

    finalEntries.push_back(&lumpData.entry);
  }

  const size_t directoryOffset = fw.Cursor() - sizeof(header);
  for (const directoryEntry *entry : finalEntries) {
    fw.WriteRaw(*entry);
  }

  // update header inf
  _header.numDirectories = static_cast<uint32_t>(finalEntries.size());
  _header.directoryOffset = static_cast<uint32_t>(directoryOffset);

  fw.SetPos(0);
  fw.WriteRaw(_header);
}



std::array<char8_t, 8> MakeLevelName(const uint16_t number)
{
  const std::string s = std::format("Map{}", number);
  return MakeLumpName(s);
}


std::array<char8_t, 8> MakeLumpName(const std::string_view name)
{
  std::array<char8_t, 8> arr{};

  const size_t len = std::min(name.size(), size_t{ 8 });
  for (size_t i = 0; i < len; ++i) {
    arr[i] = static_cast<char8_t>(name[i]);
  }

  return arr;
}
