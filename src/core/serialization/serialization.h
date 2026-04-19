#ifndef DOOMCLONE_SERIALIZATION_H
#define DOOMCLONE_SERIALIZATION_H
#include <concepts>
#include <filesystem>
#include <fstream>
#include <type_traits>
#include <utility>
#include <vector>

namespace serialization {

template<typename Writer, typename Arg>
concept HasWriteRaw = requires(Writer &writer, const Arg &data) { writer.template WriteRaw<Arg>(data); };

template<typename Reader, typename Arg>
concept HasReadRaw = requires(Reader &reader, Arg &data) {
  { reader.ReadRaw(data) } -> std::same_as<void>;
};

template<typename Writer, typename Data>
void serialize(Writer &writer, const Data &data)
  requires HasWriteRaw<Writer, Data>
{
  writer.WriteRaw(data);
}

template<typename Reader, typename Data>
void deserialize(Reader &reader, Data &data)
  requires HasReadRaw<Reader, Data>
{
  reader.ReadRaw(data);
}

}// namespace serialization

class VectorWriter
{
public:
  explicit VectorWriter(std::vector<uint8_t> &_buffer) : buffer(_buffer) {}

  std::vector<uint8_t> &buffer;

public:
  template<typename T>
  void WriteRaw(const T &data)
  {
    static_assert(std::is_trivially_copyable_v<T>);

    const uint8_t *src = reinterpret_cast<const uint8_t *>(&data);
    buffer.insert(buffer.end(), src, src + sizeof(T));
  }

  template<typename T>
  void WriteVector(const std::vector<T> &data, const bool writeSize = true)
  {
    if (writeSize) {
      WriteRaw(data.size());
    }

    for (const T &elem : data) {
      elem.serialize(*this);
    }
  }
};

class FileWriter
{
public:
  explicit FileWriter(const std::filesystem::path &path, std::ios::openmode _openmode);
  explicit FileWriter(const std::filesystem::path &path);
  explicit FileWriter();
  ~FileWriter();

  bool IsStreamGood() const { return m_fos && !m_fos.fail(); }
  template<typename T>
  void WriteRaw(const T &data)
  {
    WriteData(reinterpret_cast<const char *>(&data), sizeof(T));
  }

  void Skip(const size_t bytesToSkip) { m_fos.seekp(bytesToSkip, std::ios::cur); }
  size_t Cursor() { return m_fos.tellp(); }
  void SetPos(const size_t pos) { m_fos.seekp(pos, std::ios::beg); }

  template<typename T>
  void WriteVector(const std::vector<T> &vec, const bool writeSize = true)
  {
    if (writeSize) {
      WriteRaw(static_cast<uint64_t>(vec.size()));
    }

    for (const T &elem : vec) {
      elem.serialize(*this);
    }
  }

  template<typename T>
  void WriteObject(T &obj)
  {
    obj.serialize(*this);
  }

  void WriteData(const char *data, const size_t size)
  {
    if (IsStreamGood()) {
      m_fos.write(data, size);
    }
  }

  std::vector<uint8_t> GetBuffer() const { return m_buffer; }

private:
  std::ofstream m_fos;
  std::vector<uint8_t> m_buffer;
};

class FileReader
{
public:
  explicit FileReader(const std::filesystem::path &path);
  ~FileReader();

  bool IsStreamGood() const { return !m_fis.fail(); }
  template<typename T>
  void ReadRaw(T &type)
  {
    ReadData((char *)&type, sizeof(T));
  }

  void Skip(const size_t bytesToSkip) { m_fis.seekg(bytesToSkip, std::ios::cur); }

  size_t Cursor() { return m_fis.tellg(); }
  void SetPos(const size_t pos) { m_fis.seekg(pos, std::ios::beg); }

  template<typename T>
  void ReadObject(T &obj)
  {
    obj.deserialize(*this);
  }

  template<typename T>
  void ReadVector(std::vector<T> &vec, const bool readSize = true)
  {
    size_t n;
    if (readSize) {
      ReadRaw(n);
      vec.reserve(n);
    }

    for (size_t i = 0; IsStreamGood() && (!readSize || (readSize && i < n)); i++) {
      T val{};
      if constexpr (std::is_trivial_v<T>) {
        ReadRaw(val);
      } else {
        val.deserialize(*this);
      }
      vec.emplace_back(std::move(val));
    }
  }

  void ReadData(char *data, const size_t size)
  {
    if (IsStreamGood()) {
      m_fis.read(data, size);
    }
  }

  size_t GetFileSize()
  {
    const size_t currPos = Cursor();
    const size_t size = m_fis.seekg(std::ios::end).tellg();
    SetPos(currPos);

    return size;
  }

private:
  std::ifstream m_fis;
};

struct Serializable
{
  Serializable() = default;
  virtual ~Serializable() = default;

  virtual void serialize(FileWriter &fw) const
  {
    throw std::runtime_error("serialize method is unimplemented for the custom type.");
  }
  virtual void deserialize(FileReader &fr)
  {
    throw std::runtime_error("deserialize method is unimplemented for the custom type.");
  }

  static size_t serializationSize() { return 0; }
};

#endif// DOOMCLONE_SERIALIZATION_H
