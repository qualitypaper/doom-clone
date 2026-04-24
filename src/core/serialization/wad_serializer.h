#ifndef DOOMCLONE_WAD_PARSER_H
#define DOOMCLONE_WAD_PARSER_H

#include "defs.h"
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
  [[nodiscard]] LumpData ReadLump(const directoryEntry &entry) const;

  static LumpData ReadLump(FileReader &fr, const directoryEntry &entry);
  static std::vector<LumpData> ReadLumps(FileReader &fr, const std::vector<directoryEntry> &entries);
  void LoadAllLumps(FileReader &fr);

  [[nodiscard]] const header &GetHeader() const;
  [[nodiscard]] const std::vector<directoryEntry> &GetDirectory() const;
  [[nodiscard]] std::vector<directoryEntry> &GetDirectory();
  [[nodiscard]] const std::vector<LumpData> &GetLoadedLumps() const;
  [[nodiscard]] std::vector<LumpData> &GetLoadedLumps();

  [[nodiscard]] std::optional<directoryEntry *> FindDirectoryEntry(const lumpName &name);
  [[nodiscard]] const LumpData *FindLoadedLump(const lumpName &name) const;

  void SetHeader(const header &hdr);
  void SetLumps(std::vector<LumpData> lumps);
  void Write(const std::filesystem::path &filePath, header *hdrOverride = nullptr);
  void Write(header *hdrOverride = nullptr);

  static lumpName MakeLumpName(std::string_view name);

private:
  static std::vector<directoryEntry> ReadDirectory(FileReader &fr, const header &hdr);
  static void WriteLumpData(FileWriter &fw, LumpData &lumpData);
  static void WriteLumpsToPath(std::vector<LumpData> &allLumps, header &hdr, const std::filesystem::path &filePath);

private:
  std::filesystem::path m_filePath;
  header m_header{};
  std::vector<directoryEntry> m_directory;
  std::vector<LumpData> m_loadedLumps;
};


#endif// DOOMCLONE_WAD_PARSER_H
