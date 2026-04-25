#ifndef DOOMCLONE_TEX_DEFS_H
#define DOOMCLONE_TEX_DEFS_H

#include "defs.h"
#include "core/serialization/serialization.h"

#include <cstdint>
#include <vector>

struct Post
{
  uint8_t topDelta{};
  std::vector<uint8_t> pixels;

  template<typename Writer>
  void serialize(Writer &w) const
  {
    serialization::serialize(w, topDelta);
    serialization::serialize(w, pixels.size());
    for (const auto pixel : pixels) {
      serialization::serialize(w, pixel);
    }
  }

  template<typename Reader>
  void deserialize(Reader &r)
  {
    serialization::deserialize(r, topDelta);
    size_t pixelsSize;
    serialization::deserialize(r, pixelsSize);
    std::vector<uint8_t> pixelsVec(pixelsSize);
    for (size_t i = 0; i < pixelsSize; i++) {
      serialization::deserialize(r, pixelsVec[i]);
    }
    pixels = std::move(pixelsVec);
  }
};

struct Patch
{
  uint16_t width, height;
  int16_t leftOffset, topOffset;
  // length - width
  std::vector<Post> posts;

  template<typename Writer>
  void serialize(Writer &w) const
  {
    serialization::serialize(w, width);
    serialization::serialize(w, height);
    serialization::serialize(w, leftOffset);
    serialization::serialize(w, topOffset);
    serialization::serialize(w, posts.size());
    for (const auto &post : posts) {
      post.serialize(w);
    }
  }

  template<typename Reader>
  void deserialize(Reader &r)
  {
    serialization::deserialize(r, width);
    serialization::deserialize(r, height);
    serialization::deserialize(r, leftOffset);
    serialization::deserialize(r, topOffset);
    size_t postsSize;
    serialization::deserialize(r, postsSize);
    posts.resize(postsSize);
    for (size_t i = 0; i < postsSize; i++) {
      posts[i].deserialize(r);
    }
  }
};

struct MapPatch
{
  uint16_t originX, originY;
  uint16_t patch;

  template<typename Writer>
  void serialize(Writer &w) const
  {
    serialization::serialize(w, originX);
    serialization::serialize(w, originY);
    serialization::serialize(w, patch);
  }

  template<typename Reader>
  void deserialize(Reader &r)
  {
    serialization::deserialize(r, originX);
    serialization::deserialize(r, originY);
    serialization::deserialize(r, patch);
  }
};

struct MapTexture
{
  lumpName name;
  bool masked;
  uint16_t width, height;
  std::vector<MapPatch> patches;

  template<typename Writer>
  void serialize(Writer &w) const
  {
    serialization::serialize(w, name);
    serialization::serialize(w, masked);
    serialization::serialize(w, width);
    serialization::serialize(w, height);
    serialization::serialize(w, patches.size());
    for (const auto &patch : patches) {
      patch.serialize(w);
    }
  }

  template<typename Reader>
  void deserialize(Reader &r)
  {
    serialization::deserialize(r, name);
    serialization::deserialize(r, masked);
    serialization::deserialize(r, width);
    serialization::deserialize(r, height);
    size_t patchesSize;
    serialization::deserialize(r, patchesSize);
    patches.resize(patchesSize);
    for (size_t i = 0; i < patchesSize; i++) {
      patches[i].deserialize(r);
    }
  }
};

struct texpatch_t
{
  int16_t originx;
  int16_t originy;
  Patch patch;
};

struct texture_t
{
  lumpName name;
  bool masked;
  uint16_t width, height;
  std::vector<texpatch_t> patches;
};


#endif// DOOMCLONE_TEX_DEFS_H
