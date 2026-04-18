#ifndef DOOMCLONE_WAD_PARSER_H
#define DOOMCLONE_WAD_PARSER_H

#include "serialization.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

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

class WadSerializer
{
public:
  WadSerializer() = default;
  explicit WadSerializer(std::filesystem::path filePath);

  void SetFilePath(const std::filesystem::path &filePath);
  [[nodiscard]] const std::filesystem::path &GetFilePath() const;

  bool Load(bool readLumps = false);
  bool LoadHeader(FileReader &fr);
  bool LoadDirectory(FileReader &fr);

  static LumpData ReadLump(FileReader &fr, const directoryEntry &entry);
  static std::vector<LumpData> ReadLumps(FileReader &fr, const std::vector<directoryEntry> &entries);
  void LoadAllLumps(FileReader &fr);

  [[nodiscard]] const header &GetHeader() const;
  [[nodiscard]] const std::vector<directoryEntry> &GetDirectory() const;
  [[nodiscard]] const std::vector<LumpData> &GetLoadedLumps() const;

  [[nodiscard]] std::optional<directoryEntry> FindDirectoryEntry(const std::array<char8_t, 8> &name) const;
  [[nodiscard]] const LumpData *FindLoadedLump(const std::array<char8_t, 8> &name) const;

  void SetHeader(const header &hdr);
  void SetLumps(std::vector<LumpData> lumps);
  void Write(const std::filesystem::path &filePath, header *hdrOverride = nullptr);
  void Write(header *hdrOverride = nullptr);

  static std::array<char8_t, 8> MakeLevelName(uint16_t number);
  static std::array<char8_t, 8> MakeLumpName(std::string_view name);

private:
  static std::vector<directoryEntry> ReadDirectory(FileReader &fr, const header &hdr);
  static void WriteLump(FileWriter &fw, LumpData &lumpData);
  static void WriteLumpsToPath(std::vector<LumpData> &allLumps, header &hdr, const std::filesystem::path &filePath);

private:
  std::filesystem::path m_filePath;
  header m_header{};
  std::vector<directoryEntry> m_directory;
  std::vector<LumpData> m_loadedLumps;
};


#endif// DOOMCLONE_WAD_PARSER_H
