#ifndef DOOMCLONE_TEXTURE_SERIALIZER_H
#define DOOMCLONE_TEXTURE_SERIALIZER_H

#include "defs.h"
#include "image_converter.h"
#include "wad_serializer.h"

#include <SDL_render.h>
#include <algorithm>
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
  explicit WallTexture(MapTexture _mapTexture) : mapTexture(std::move(_mapTexture))
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
                 std::vector<PatchView> _patches,
                 std::vector<WallTexture> _wallTextures,
                 PaletteManager *_palManager)
    : palManager(_palManager), m_flatTextures(std::move(_flatTextures)), m_patches(std::move(_patches)),
      m_wallTextures(std::move(_wallTextures))
  {}

  PaletteManager *palManager = nullptr;

  void LoadFlatTexture(const std::filesystem::path &path, FlatTexture &outFlatTexture) const;
  void LoadWallTexture(const std::filesystem::path &path, PatchView &outWallTexture) const;
  void ConvertFlatToWall(FlatTexture &texture);

  [[nodiscard]] const FlatTexture *GetFlatTexture(size_t index) const;
  [[nodiscard]] std::string GetFlatTextureName(size_t index) const;

  [[nodiscard]] const WallTexture *GetWallTexture(size_t index) const;
  [[nodiscard]] const PatchView * GetPatch(size_t index) const;

  [[nodiscard]] const std::vector<WallTexture> &GetWallTextures() const
  {
    return m_wallTextures;
  }
  [[nodiscard]] const std::vector<FlatTexture> &GetFlatTextures() const
  {
    return m_flatTextures;
  }
  [[nodiscard]] const std::vector<PatchView> &GetPatches() const
  {
    return m_patches;
  }

  [[nodiscard]] std::vector<WallTexture> &GetWallTextures()
  {
    return m_wallTextures;
  }
  [[nodiscard]] std::vector<FlatTexture> &GetFlatTextures()
  {
    return m_flatTextures;
  }
  [[nodiscard]] std::vector<PatchView> &GetPatches()
  {
    return m_patches;
  }

  void AddDefaultFlatTexture();
  void AddDefaultPatch();
  void AddDefaultWallTexture();
  std::string_view GetPatchTextureName(int16_t patchIndex);

  void WritePNames(WadSerializer &wadSerializer) const;
  void WriteWallTexture(const WallTexture &value) const;

private:
  std::vector<FlatTexture> m_flatTextures;
  std::vector<PatchView> m_patches;
  std::vector<WallTexture> m_wallTextures;
  size_t m_prevPatchesSize = 0;
};

bool LoadTextureFromMemory(const void *data, int width, int height, SDL_Renderer *renderer, SDL_Texture **out_texture);

#endif// DOOMCLONE_TEXTURE_SERIALIZER_H
