#ifndef DOOMCLONE_TEXTURE_SERIALIZER_H
#define DOOMCLONE_TEXTURE_SERIALIZER_H

#include "defs.h"
#include "wad_serializer.h"

#include <algorithm>
#include <cstdint>
#include <vector>

struct Texture
{
  explicit Texture(const lumpName &_name) : name(_name) {}

  virtual ~Texture() = default;

  virtual void Write() {}
  virtual void Read() {}

  lumpName name;
  std::vector<uint8_t> data;
};

class FlatTexture : public Texture
{
public:
  explicit FlatTexture(const lumpName &_name) : Texture(_name) {}

  void Write() override;
  void Read() override;

  static std::vector<FlatTexture> ReadAll();
};

class WallTexture : public Texture
{
public:
  explicit WallTexture(const lumpName &_name) : Texture(_name) {}

  void Write() override;
  void Read() override;
};


#endif// DOOMCLONE_TEXTURE_SERIALIZER_H
