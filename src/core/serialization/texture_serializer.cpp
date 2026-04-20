#include "texture_serializer.h"
#include "config.h"
#include "std_image.h"

#include <SDL_render.h>
#include <SDL_surface.h>
#include <iostream>

static constexpr std::string_view DEFAULT_FLAT_TEX_FORMAT = "FTEX{}";

bool LoadTextureFromMemory(const void *data,
                           const int width,
                           const int height,
                           SDL_Renderer *renderer,
                           SDL_Texture **out_texture)
{
  constexpr int channels = 4;

  SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(const_cast<void *>(data),
                                                  width,
                                                  height,
                                                  channels * 8,
                                                  channels * width,
                                                  0xff000000,
                                                  0x00ff0000,
                                                  0x0000ff00,
                                                  0x000000ff);
  if (surface == nullptr) {
    fprintf(stderr, "Failed to create SDL surface: %s\n", SDL_GetError());
    return false;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (texture == nullptr)
    fprintf(stderr, "Failed to create SDL texture: %s\n", SDL_GetError());

  *out_texture = texture;

  SDL_FreeSurface(surface);

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

  const auto it = std::ranges::find_if(lumps, [&](const LumpData &lump) {
    return lump.entry.name == name;
  });
  const int index = it - lumps.begin();

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

  if (it == lumps.end()) {
    this->data = std::move(wadSerializer.GetLoadedLumps().back().rawData);
  } else {
    this->data = std::move(wadSerializer.GetLoadedLumps()[index].rawData);
  }
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
    FlatTexture &flatTexture = res.emplace_back(curr->name);
    flatTexture.data = std::move(wadSerializer.ReadLump(*curr).rawData);
    curr++;
  }

  return std::move(res);
}

// TODO: implement texture read/write for walls
void WallTexture::Write()
{}

void WallTexture::Read()
{}

void TextureManager::LoadFlatTexture(const std::filesystem::path &path, FlatTexture &outFlatTexture) const
{
  assert(palManager);
  const RawImage &rawImage = image::LoadRawImage(path, *palManager);
  std::vector<uint8_t> rawData = image::ConvertRawToFlat(rawImage);

  outFlatTexture.data = std::move(rawData);
  outFlatTexture.width = outFlatTexture.height = FLAT_TEXTURE_SIZE;
}

void TextureManager::ConvertFlatToWall(FlatTexture &texture)
{
  const auto &it = std::ranges::find_if(flatTextures, [&texture](const FlatTexture &ft) {
    return ft.name == texture.name;
  });
  if (it == flatTextures.end()) {
    std::cerr << "Texture: " << (char*) texture.name.data() << " wasn't found\n";
    return;
  }

  WallTexture wallTexture{ texture.name };

  // TODO: convert into wall data format
  wallTexture.data = std::move(texture.data);
  wallTexture.width = wallTexture.height = 64;

  wallTextures.emplace_back(wallTexture);
  flatTextures.erase(it);
}

void TextureManager::ConvertFlatToWall(const size_t index)
{
  if (index >= flatTextures.size()) {
    std::cerr << "Index is bigger than flatTextures array: " << index << '\n';
    return;
  }

  ConvertFlatToWall(flatTextures[index]);
}

void TextureManager::ConvertWallToFlat(const size_t index)
{
  if (index >= wallTextures.size())
    return;

  WallTexture &wallTexture = wallTextures.at(index);
  FlatTexture flatTexture{ wallTexture.name };

  // TODO: convert into flat data format
  flatTexture.data = std::move(wallTexture.data);
  flatTextures.emplace_back(flatTexture);

  wallTextures.erase(wallTextures.begin() + index);
}

std::optional<FlatTexture *> TextureManager::GetFlatTexture(const size_t index)
{
  if (index >= flatTextures.size())
    return {};

  return &flatTextures.at(index);
}

std::string TextureManager::GetFlatTextureName(const size_t index)
{
  std::string textureName = GetFlatTexture(index)
                              .and_then([](FlatTexture *ft) {
                                return std::optional(std::string((char *)ft->name.data()));
                              })
                              .value_or("###");
  return std::move(textureName);
}
void TextureManager::AddDefaultFlatTexture()
{
  flatTextures.emplace_back(WadSerializer::MakeLumpName(std::format(DEFAULT_FLAT_TEX_FORMAT, flatTextures.size())));
}
