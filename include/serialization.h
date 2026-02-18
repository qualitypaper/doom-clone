#ifndef DOOMCLONE_SERIALIZATION_H
#define DOOMCLONE_SERIALIZATION_H
#include <filesystem>
#include <fstream>
#include <vector>

class FileWriter
{
public:
  explicit FileWriter(const std::filesystem::path &path);
  ~FileWriter();

  bool IsStreamGood() const { return !m_fos.fail(); }
  template<typename T> void WriteRaw(const T &data) { WriteData(reinterpret_cast<const char *>(&data), sizeof(T)); }

  template<typename T> void WriteVector(const std::vector<T> &vec, const bool writeSize = true)
  {
    if (writeSize) { WriteRaw((uint64_t)vec.size()); }

    for (auto &elem : vec) { elem.serialize(*this); }
  }

  template<typename T> void WriteObject(T &obj) { obj.serialize(*this); }

  void WriteData(const char *data, const size_t size)
  {
    if (IsStreamGood()) { m_fos.write(data, size); }
  }

private:
  std::ofstream m_fos;
};

class FileReader
{
public:
  explicit FileReader(const std::filesystem::path &path);
  ~FileReader();

  bool IsStreamGood() const { return !m_fis.fail(); }
  template<typename T> void ReadRaw(T &type) { ReadData((char *)&type, sizeof(T)); }

  template<typename T> void ReadObject(T &obj) { obj.deserialize(this); }

  template<typename T> void ReadVector(std::vector<T> &vec, const bool readSize = true)
  {
    size_t n;
    if (readSize) {
      ReadRaw(n);
      vec.reserve(n);
    }

    for (size_t i = 0; IsStreamGood() && (!readSize || (readSize && i < n)); i++) {
      T val{};
      if (std::is_trivial<T>()) {
        ReadRaw(val);
      } else {
        val.deserialize(*this);
      }
      vec.emplace_back(val);
    }
  }

  void ReadData(char *data, const size_t size)
  {
    if (IsStreamGood()) { m_fis.read(data, size); }
  }

private:
  std::ifstream m_fis;
};

struct Serializable
{
  Serializable() = default;
  virtual ~Serializable() = default;

  virtual void serialize(FileWriter &fw) const {}
  virtual void deserialize(FileReader &fr) {}
};

#endif// DOOMCLONE_SERIALIZATION_H
