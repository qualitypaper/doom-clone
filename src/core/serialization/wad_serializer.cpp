#include "wad_serializer.h"

#include <algorithm>
#include <format>

WadSerializer::WadSerializer(std::filesystem::path filePath) : m_filePath(std::move(filePath)) {}

void WadSerializer::SetFilePath(const std::filesystem::path &filePath)
{
  m_filePath = filePath;
}

const std::filesystem::path &WadSerializer::GetFilePath() const
{
  return m_filePath;
}

bool WadSerializer::Load(const bool readLumps)
{
  if (m_filePath.empty()) {
    return false;
  }

  FileReader fr(m_filePath);
  if (!fr.IsStreamGood()) {
    return false;
  }

  if (!LoadHeader(fr) || !LoadDirectory(fr)) {
    return false;
  }

  if (readLumps) {
    LoadAllLumps(fr);
  } else {
    m_loadedLumps.clear();
  }

  return true;
}

bool WadSerializer::LoadHeader(FileReader &fr)
{
  if (!fr.IsStreamGood()) {
    return false;
  }

  fr.SetPos(0);
  fr.ReadRaw(m_header);
  return fr.IsStreamGood();
}

bool WadSerializer::LoadDirectory(FileReader &fr)
{
  if (!fr.IsStreamGood()) {
    return false;
  }

  m_directory = ReadDirectory(fr, m_header);
  return fr.IsStreamGood();
}

LumpData WadSerializer::ReadLump(FileReader &fr, const directoryEntry &entry)
{
  LumpData lumpData;
  lumpData.entry = entry;

  fr.SetPos(entry.offset);

  lumpData.rawData.resize(entry.size);
  fr.ReadData(reinterpret_cast<char *>(lumpData.rawData.data()), entry.size);

  return lumpData;
}

std::vector<LumpData> WadSerializer::ReadLumps(FileReader &fr, const std::vector<directoryEntry> &entries)
{
  std::vector<LumpData> lumps;
  lumps.reserve(entries.size());

  for (const directoryEntry &entry : entries) {
    lumps.push_back(ReadLump(fr, entry));
  }

  return lumps;
}

void WadSerializer::LoadAllLumps(FileReader &fr)
{
  m_loadedLumps = ReadLumps(fr, m_directory);
}

const header &WadSerializer::GetHeader() const
{
  return m_header;
}

const std::vector<directoryEntry> &WadSerializer::GetDirectory() const
{
  return m_directory;
}

const std::vector<LumpData> &WadSerializer::GetLoadedLumps() const
{
  return m_loadedLumps;
}

std::optional<directoryEntry> WadSerializer::FindDirectoryEntry(const std::array<char8_t, 8> &name) const
{
  const auto it = std::find_if(m_directory.begin(), m_directory.end(), [&](const directoryEntry &entry) {
    return entry.name == name;
  });

  if (it == m_directory.end()) {
    return std::nullopt;
  }

  return *it;
}

const LumpData *WadSerializer::FindLoadedLump(const std::array<char8_t, 8> &name) const
{
  const auto it = std::find_if(m_loadedLumps.begin(), m_loadedLumps.end(), [&](const LumpData &lumpData) {
    return lumpData.entry.name == name;
  });

  if (it == m_loadedLumps.end()) {
    return nullptr;
  }

  return &(*it);
}

void WadSerializer::SetHeader(const header &hdr)
{
  m_header = hdr;
}

void WadSerializer::SetLumps(std::vector<LumpData> lumps)
{
  m_loadedLumps = std::move(lumps);
  m_directory.clear();
  m_directory.reserve(m_loadedLumps.size());

  for (const LumpData &lump : m_loadedLumps) {
    m_directory.push_back(lump.entry);
  }
}

void WadSerializer::Write(const std::filesystem::path &filePath, header *hdrOverride)
{
  if (m_loadedLumps.empty()) {
    return;
  }

  header hdrToWrite = hdrOverride != nullptr ? *hdrOverride : m_header;
  if (hdrToWrite.magicNumber == std::array<char8_t, 4>{}) {
    hdrToWrite.magicNumber = { 'P', 'W', 'A', 'D' };
  }

  auto lumpsCopy = m_loadedLumps;
  WriteLumpsToPath(lumpsCopy, hdrToWrite, filePath);

  m_loadedLumps = std::move(lumpsCopy);
  m_header = hdrToWrite;
  m_directory.clear();
  m_directory.reserve(m_loadedLumps.size());

  for (const LumpData &lumpData : m_loadedLumps) {
    m_directory.push_back(lumpData.entry);
  }
}

void WadSerializer::Write(header *hdrOverride)
{
  if (m_filePath.empty()) {
    throw std::runtime_error("WadSerializer file path is empty.");
  }

  Write(m_filePath, hdrOverride);
}

std::array<char8_t, 8> WadSerializer::MakeLevelName(const uint16_t number)
{
  const std::string s = std::format("Map{}", number);
  return MakeLumpName(s);
}

std::array<char8_t, 8> WadSerializer::MakeLumpName(const std::string_view name)
{
  std::array<char8_t, 8> arr{};

  const size_t len = std::min(name.size(), size_t{ 8 });
  for (size_t i = 0; i < len; ++i) {
    arr[i] = static_cast<char8_t>(name[i]);
  }

  return arr;
}

std::vector<directoryEntry> WadSerializer::ReadDirectory(FileReader &fr, const header &hdr)
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

void WadSerializer::WriteLump(FileWriter &fw, LumpData &lumpData)
{
  lumpData.entry.offset = static_cast<uint32_t>(fw.Cursor());
  lumpData.entry.size = static_cast<uint32_t>(lumpData.rawData.size());

  fw.WriteData(reinterpret_cast<const char *>(lumpData.rawData.data()), lumpData.rawData.size());
}

void WadSerializer::WriteLumpsToPath(std::vector<LumpData> &allLumps,
                                     header &hdr,
                                     const std::filesystem::path &filePath)
{
  FileWriter fw(filePath, std::ios::binary | std::ios::trunc);

  if (!fw.IsStreamGood()) {
    throw std::runtime_error("Failed to open file for saving.\n");
  }

  fw.WriteRaw(hdr);

  for (auto &lumpData : allLumps) {
    WriteLump(fw, lumpData);
  }

  const size_t directoryOffset = fw.Cursor() - sizeof(header);
  for (const LumpData &lumpData : allLumps) {
    fw.WriteRaw(lumpData.entry);
  }

  hdr.numDirectories = static_cast<uint32_t>(allLumps.size());
  hdr.directoryOffset = static_cast<uint32_t>(directoryOffset);

  fw.SetPos(0);
  fw.WriteRaw(hdr);
}
