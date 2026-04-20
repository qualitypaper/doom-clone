#ifndef DOOMCLONE_TEXTURE_SERIALIZER_H
#define DOOMCLONE_TEXTURE_SERIALIZER_H

#include "defs.h"
#include "image_converter.h"
#include "wad_serializer.h"

#include <SDL_render.h>
#include <algorithm>
#include <cstdint>
#include <vector>

constexpr uint16_t FLAT_TEXTURE_SIZE = 64;

enum class TextureType : int { FLAT, WALL };

struct Texture
{
  Texture(const lumpName &_name, const TextureType _type, const int _width, const int _height)
    : type(_type), name(_name), width(_width), height(_height)
  {}
  Texture(const lumpName &_name,
          std::vector<uint8_t> _data,
          const TextureType _type,
          const int _width,
          const int _height)
    : type(_type), name(_name), data(std::move(_data)), width(_width), height(_height)
  {}

  virtual ~Texture() = default;

  virtual void Write()
  {}
  virtual void Read()
  {}

  TextureType type;
  lumpName name;
  std::vector<uint8_t> data;
  int width, height;
};

class FlatTexture : public Texture
{
public:
  explicit FlatTexture(const lumpName &_name) : Texture(_name, TextureType::FLAT, FLAT_TEXTURE_SIZE, FLAT_TEXTURE_SIZE)
  {}

  FlatTexture(const lumpName &_name, std::vector<uint8_t> _data)
    : Texture(_name, TextureType::FLAT, FLAT_TEXTURE_SIZE, FLAT_TEXTURE_SIZE)
  {
    if (_data.size() != FLAT_TEXTURE_SIZE * FLAT_TEXTURE_SIZE) {
      throw std::runtime_error("Data of the flat texture is of unexpected size: " + std::to_string(_data.size())
                               + ", when 64 * 64 is expected.");
    }
    this->data = std::move(_data);
  }
  ~FlatTexture() override = default;

  void Write() override;
  void Read() override;

  static std::vector<FlatTexture> ReadAll();
};

class WallTexture : public Texture
{
public:
  explicit WallTexture(const lumpName _name) : Texture(_name, TextureType::WALL, -1, -1)
  {}
  ~WallTexture() override = default;

  void Write() override;
  void Read() override;
};

class TextureManager
{
public:
  TextureManager() = default;
  TextureManager(std::vector<FlatTexture> _flatTextures,
                 std::vector<WallTexture> _wallTextures,
                 PaletteManager *_palManager)
    : palManager(_palManager), flatTextures(std::move(_flatTextures)), wallTextures(std::move(_wallTextures))
  {}

  PaletteManager *palManager = nullptr;
  std::vector<FlatTexture> flatTextures;
  std::vector<WallTexture> wallTextures;

  void ConvertFlatToWall(size_t index);
  void ConvertWallToFlat(size_t index);

  [[nodiscard]] void LoadFlatTexture(const std::filesystem::path &path, FlatTexture &outFlatTexture) const;
  void ConvertFlatToWall(FlatTexture &texture);

  [[nodiscard]] std::optional<FlatTexture *> GetFlatTexture(size_t index);
  [[nodiscard]] std::string GetFlatTextureName(size_t index);
  void AddDefaultFlatTexture();
};

bool LoadTextureFromMemory(const void *data, int width, int height, SDL_Renderer *renderer, SDL_Texture **out_texture);

#endif// DOOMCLONE_TEXTURE_SERIALIZER_H
