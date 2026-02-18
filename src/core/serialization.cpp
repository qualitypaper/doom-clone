#include <serialization.h>

FileWriter::FileWriter(const std::filesystem::path &path) { this->m_fos = std::ofstream(path, std::ios::binary); }

FileWriter::~FileWriter() { this->m_fos.close(); }

FileReader::FileReader(const std::filesystem::path &path) { this->m_fis = std::ifstream(path, std::ios::binary); }

FileReader::~FileReader() { m_fis.close(); }
