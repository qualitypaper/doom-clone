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

enum class TextureType : int { FLAT, PATCH };

struct Texture
{
  Texture(const lumpName &_name, const TextureType _type, const int _width, const int _height)
    : type(_type), name(_name), width(_width), height(_height)
  {}

  virtual ~Texture() = default;

  virtual void Write()
  {}
  virtual void Read()
  {}

  TextureType type;
  lumpName name;
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

  std::vector<uint8_t> data;
};

// defines a wall texture, which is quite similar to a patch in Doom
struct PatchView : Texture
{
  explicit PatchView(const lumpName _name) : Texture(_name, TextureType::PATCH, -1, -1)
  {}
  ~PatchView() override = default;

  void Write() override;
  void Read() override;
  static std::vector<PatchView> ReadAll();

  int16_t topOffset{}, leftOffset{};
  std::vector<Post> data;
};

struct WallTexture
{
  explicit WallTexture(const lumpName _name) : mapTexture(_name)
  {}

  MapTexture mapTexture;

  void Write();
  void Read();

  void AddPatch()
  {
    mapTexture.patches.emplace_back(0, 0, -1);
  }

  std::vector<MapPatch> &GetPatches()
  {
    return mapTexture.patches;
  }

  static std::vector<WallTexture> ReadAll();
};

class TextureManager
{
public:
  TextureManager() = default;
  TextureManager(std::vector<FlatTexture> _flatTextures,
                 std::vector<PatchView> _wallTextures,
                 PaletteManager *_palManager)
    : palManager(_palManager), flatTextures(std::move(_flatTextures)), patches(std::move(_wallTextures))
  {}

  PaletteManager *palManager = nullptr;
  std::vector<FlatTexture> flatTextures;
  std::vector<PatchView> patches;
  std::vector<WallTexture> wallTextures;

  void LoadFlatTexture(const std::filesystem::path &path, FlatTexture &outFlatTexture) const;
  void LoadWallTexture(const std::filesystem::path &path, PatchView &outWallTexture) const;
  void ConvertFlatToWall(FlatTexture &texture);

  [[nodiscard]] std::optional<FlatTexture *> GetFlatTexture(size_t index);
  [[nodiscard]] std::string GetFlatTextureName(size_t index);

  void AddDefaultFlatTexture();
  void AddDefaultPatch();
  void AddDefaultWallTexture();
};

bool LoadTextureFromMemory(const void *data, int width, int height, SDL_Renderer *renderer, SDL_Texture **out_texture);

#endif// DOOMCLONE_TEXTURE_SERIALIZER_H
