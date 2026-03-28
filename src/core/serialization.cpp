#include <serialization.h>

FileWriter::FileWriter() : m_fos(nullptr, std::ios::binary | std::ios::out) {}

FileWriter::FileWriter(const std::filesystem::path &path, std::ios::openmode _openmode)
{ this->m_fos = std::ofstream(path, _openmode); }

FileWriter::FileWriter(const std::filesystem::path &path)
{ this->m_fos = std::ofstream(path, std::ios::binary | std::ios::out); }

FileWriter::~FileWriter() { this->m_fos.close(); }

FileReader::FileReader(const std::filesystem::path &path)
{ this->m_fis = std::ifstream(path, std::ios::binary | std::ios::in); }

FileReader::~FileReader() { m_fis.close(); }
