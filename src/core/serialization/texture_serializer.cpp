#include "texture_serializer.h"
#include "config.h"

#include <iostream>

void FlatTexture::Write()
{
  assert(data.data() && !data.empty());

  WadSerializer wadSerializer(SAVED_LEVEL_PATH);
  if (!wadSerializer.Load(true)) {
    return;
  }

  lumpName fStartName = WadSerializer::MakeLumpName("F_START");
  lumpName fEndName = WadSerializer::MakeLumpName("F_END");

  const std::optional<directoryEntry *> &fStartOptional = wadSerializer.FindDirectoryEntry(fStartName);

  size_t fEndIndex = 0;

  if (!fStartOptional.has_value()) {
    std::vector<directoryEntry> &directory = wadSerializer.GetDirectory();
    size_t offset;

    if (directory.empty()) {
      offset = 0;
    } else {
      offset = directory.back().offset + sizeof(directoryEntry);
    }

    directory.emplace_back(offset, 0, fStartName);
    directory.emplace_back(offset + sizeof(directoryEntry), 0, fEndName);

    fEndIndex = directory.size() - 1;
  } else {
    const directoryEntry *directory = wadSerializer.GetDirectory().data();
    const auto &fEndOptional = wadSerializer.FindDirectoryEntry(fEndName);

    fEndIndex = fEndOptional.value() - directory;
  }

  std::vector<LumpData> &lumps = wadSerializer.GetLoadedLumps();
  const auto it = std::ranges::find_if(lumps, [&](const LumpData &lump) { return lump.entry.name == name; });

  LumpData flatLump{};
  flatLump.entry.name = name;
  flatLump.rawData = std::move(data);

  if (it == lumps.end()) {
    lumps.insert(lumps.begin() + fEndIndex, std::move(flatLump));
  } else {
    *it = std::move(flatLump);
  }

  wadSerializer.SetLumps(std::move(lumps));
  wadSerializer.Write();
}

void FlatTexture::Read()
{
  WadSerializer wadSerializer(SAVED_LEVEL_PATH);

  if (!wadSerializer.Load(false)) {
    return;
  }

  const std::optional<directoryEntry *> &entry = wadSerializer.FindDirectoryEntry(name);

  if (!entry.has_value()) {
    std::cerr << "Couldn't find flat texture with name: " << (char *)name.data() << '\n';
    return;
  }

  FileReader fr(SAVED_LEVEL_PATH);
  LumpData flatLump = WadSerializer::ReadLump(fr, *entry.value());

  this->data = std::move(flatLump.rawData);
}

std::vector<FlatTexture> FlatTexture::ReadAll()
{
  WadSerializer wadSerializer(SAVED_LEVEL_PATH);

  if (!wadSerializer.Load(false)) {
    return {};
  }
  const lumpName fStartName = WadSerializer::MakeLumpName("F_START");

  const std::optional<directoryEntry *> fStartOptional = wadSerializer.FindDirectoryEntry(fStartName);
  if (!fStartOptional.has_value()) {
    return {};
  }

  const lumpName fEndName = WadSerializer::MakeLumpName("F_END");
  const std::optional<directoryEntry *> &fEndOptional = wadSerializer.FindDirectoryEntry(fEndName);

  std::vector<FlatTexture> res;
  res.reserve(fEndOptional.value() - fStartOptional.value());

  directoryEntry *curr = fStartOptional.value() + 1;

  while (curr != fEndOptional.value()) {
    res.emplace_back(curr->name);
    curr++;
  }

  return std::move(res);
}

// TODO: implement texture read/write for walls