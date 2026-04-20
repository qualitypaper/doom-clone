#include "texture_serializer.h"
#include "config.h"
#include "std_image.h"

#include <SDL_render.h>
#include <SDL_surface.h>
#include <iostream>

bool LoadTextureFromFile(const char *fileName,
                         SDL_Renderer *renderer,
                         SDL_Texture **out_texture,
                         int *outWidth,
                         int *outHeight)
{
  FileReader fr(fileName);
  if (!fr.IsStreamGood()) {
    std::cout << std::format("Failed to open texture file {}\n", fileName);
  }

  const size_t size = fr.GetFileSize();
  char buf[size];
  fr.ReadData(buf, size);

  return LoadTextureFromMemory(buf, size, renderer, out_texture, outWidth, outHeight);
}

bool LoadTextureFromMemory(const void *data,
                           const size_t data_size,
                           SDL_Renderer *renderer,
                           SDL_Texture **out_texture,
                           int *out_width,
                           int *out_height)
{
  int image_width = 64;
  int image_height = 64;
  int channels = 4;
  // unsigned char *image_data =
  //   stbi_load_from_memory((const unsigned char *)data, (int)data_size, &image_width, &image_height, nullptr, channels);
  //
  // if (image_data == nullptr) {
  //   fprintf(stderr, "Failed to load image: %s\n", stbi_failure_reason());
  //   return false;
  // }
  //
  SDL_Surface *surface = SDL_CreateRGBSurfaceFrom((void *)data,
                                                  image_width,
                                                  image_height,
                                                  channels * 8,
                                                  channels * image_width,
                                                  0x000000ff,
                                                  0x0000ff00,
                                                  0x00ff0000,
                                                  0xff000000);
  if (surface == nullptr) {
    fprintf(stderr, "Failed to create SDL surface: %s\n", SDL_GetError());
    return false;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (texture == nullptr)
    fprintf(stderr, "Failed to create SDL texture: %s\n", SDL_GetError());

  *out_texture = texture;
  *out_width = image_width;
  *out_height = image_height;

  SDL_FreeSurface(surface);
  // stbi_image_free(image_data);

  return true;
}

void FlatTexture::Write()
{
  WadSerializer wadSerializer(SAVED_LEVEL_PATH);
  if (!wadSerializer.Load(true)) {
    return;
  }

  std::vector<LumpData> &lumps = wadSerializer.GetLoadedLumps();

  lumpName fStartName = WadSerializer::MakeLumpName("F_START");
  lumpName fEndName = WadSerializer::MakeLumpName("F_END");

  const std::optional<directoryEntry *> &fStartOptional = wadSerializer.FindDirectoryEntry(fStartName);

  size_t fEndIndex = 0;

  if (!fStartOptional.has_value()) {
    std::vector<directoryEntry> &directory = wadSerializer.GetDirectory();

    directoryEntry fStart = directory.emplace_back(0, 0, fStartName);
    // making a buffer for the new directory entry
    directoryEntry fEnd = directory.emplace_back(0, 0, fEndName);

    lumps.emplace_back(std::vector<uint8_t>(), fStart);
    lumps.emplace_back(std::vector<uint8_t>(), fEnd);

    // using this index for lumps before adding fStart/fEnd so both of them must be skipped
    fEndIndex = directory.size() - 1;
  } else {
    const directoryEntry *directory = wadSerializer.GetDirectory().data();
    const auto &fEndOptional = wadSerializer.FindDirectoryEntry(fEndName);

    fEndIndex = fEndOptional.value() - directory;
  }

  const auto it = std::ranges::find_if(lumps, [&](const LumpData &lump) { return lump.entry.name == name; });

  LumpData flatLump{};
  flatLump.entry.name = name;
  flatLump.rawData = std::move(data);

  if (it == lumps.end()) {
    lumps.insert(lumps.begin() + fEndIndex, std::move(flatLump));
  } else {
    *it = std::move(flatLump);
  }

  wadSerializer.SetLumps(std::move(lumps));
  wadSerializer.Write();
}

void FlatTexture::Read()
{
  WadSerializer wadSerializer(SAVED_LEVEL_PATH);

  if (!wadSerializer.Load(false)) {
    return;
  }

  const std::optional<directoryEntry *> &entry = wadSerializer.FindDirectoryEntry(name);

  if (!entry.has_value()) {
    std::cerr << "Couldn't find flat texture with name: " << (char *)name.data() << '\n';
    return;
  }

  FileReader fr(SAVED_LEVEL_PATH);
  LumpData flatLump = WadSerializer::ReadLump(fr, *entry.value());

  this->data = std::move(flatLump.rawData);
}

std::vector<FlatTexture> FlatTexture::ReadAll()
{
  WadSerializer wadSerializer(SAVED_LEVEL_PATH);

  if (!wadSerializer.Load(false)) {
    return {};
  }
  const lumpName fStartName = WadSerializer::MakeLumpName("F_START");

  const std::optional<directoryEntry *> fStartOptional = wadSerializer.FindDirectoryEntry(fStartName);
  if (!fStartOptional.has_value()) {
    return {};
  }

  const lumpName fEndName = WadSerializer::MakeLumpName("F_END");
  const std::optional<directoryEntry *> &fEndOptional = wadSerializer.FindDirectoryEntry(fEndName);

  std::vector<FlatTexture> res;
  res.reserve(fEndOptional.value() - fStartOptional.value());

  directoryEntry *curr = fStartOptional.value() + 1;

  while (curr != fEndOptional.value()) {
    FlatTexture & flatTexture = res.emplace_back(curr->name);
    flatTexture.data = std::move(wadSerializer.ReadLump(*curr).rawData);
    curr++;
  }

  return std::move(res);
}

// TODO: implement texture read/write for walls