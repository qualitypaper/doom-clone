#ifndef DOOMCLONE_TEXTURE_SERIALIZER_H
#define DOOMCLONE_TEXTURE_SERIALIZER_H

#include "defs.h"
#include "wad_serializer.h"

#include <SDL_render.h>
#include <algorithm>
#include <cstdint>
#include <vector>

enum class TextureType : int { FLAT, WALL };

struct Texture
{
  explicit Texture(const lumpName &_name, const TextureType _type) : type(_type), name(_name) {}

  virtual ~Texture() = default;

  virtual void Write() {}
  virtual void Read() {}

  TextureType type;
  lumpName name;
  std::vector<uint8_t> data;
};

class FlatTexture : public Texture
{
public:
  explicit FlatTexture(const lumpName &_name) : Texture(_name, TextureType::FLAT) {}
  ~FlatTexture() override = default;

  void Write() override;
  void Read() override;

  static std::vector<FlatTexture> ReadAll();
};

class WallTexture : public Texture
{
public:
  explicit WallTexture(const lumpName &_name) : Texture(_name, TextureType::WALL) {}
  ~WallTexture() override = default;

  void Write() override;
  void Read() override;
};

bool LoadTextureFromFile(const char *fileName,
                         SDL_Renderer *renderer,
                         SDL_Texture **out_texture,
                         int *outWidth,
                         int *outHeight);
bool LoadTextureFromMemory(const void *data,
                           size_t data_size,
                           SDL_Renderer *renderer,
                           SDL_Texture **out_texture,
                           int *out_width,
                           int *out_height);

#endif// DOOMCLONE_TEXTURE_SERIALIZER_H
