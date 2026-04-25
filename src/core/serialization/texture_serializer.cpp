#include "texture_serializer.h"
#include "config.h"
#include "std_image.h"

#include <SDL_render.h>
#include <SDL_surface.h>
#include <iostream>

static constexpr std::string_view DEFAULT_FLAT_TEX_FORMAT = "FTEX{}";
static constexpr std::string_view DEFAULT_PATCH_FORMAT = "PTH{}";
static constexpr std::string_view DEFAULT_WALL_TEXTURE_FORMAT = "WTEX{}";

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
  WadSerializer wadSerializer(DATA_PATH);
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
  WadSerializer wadSerializer(DATA_PATH);

  if (!wadSerializer.Load(false)) {
    return;
  }

  const std::optional<directoryEntry *> &entry = wadSerializer.FindDirectoryEntry(name);

  if (!entry.has_value()) {
    std::cerr << "Couldn't find flat texture with name: " << (char *)name.data() << '\n';
    return;
  }

  FileReader fr(DATA_PATH);
  LumpData flatLump = WadSerializer::ReadLump(fr, *entry.value());

  this->data = std::move(flatLump.rawData);
}

std::vector<FlatTexture> FlatTexture::ReadAll()
{
  WadSerializer wadSerializer(DATA_PATH);

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

void PatchView::Write()
{
  WadSerializer wadSerializer(DATA_PATH);

  if (!wadSerializer.Load(true)) {
    throw std::runtime_error("Couldn't read a data file while reading a wall texture.");
  }

  std::vector<LumpData> &lumps = wadSerializer.GetLoadedLumps();

  size_t index = 0;
  // new texture
  const std::optional<directoryEntry *> &pEndOptional =
    wadSerializer.FindDirectoryEntry(WadSerializer::MakeLumpName("P_END"));

  if (!pEndOptional.has_value()) {
    std::vector<directoryEntry> &directory = wadSerializer.GetDirectory();

    directoryEntry &pStart = directory.emplace_back(0, 0, WadSerializer::MakeLumpName("P_START"));
    directoryEntry &pEnd = directory.emplace_back(0, 0, WadSerializer::MakeLumpName("P_END"));

    lumps.emplace_back(std::vector<uint8_t>(), pStart);
    lumps.emplace_back(std::vector<uint8_t>(), pEnd);

    index = directory.size() - 1;
  } else {
    index = pEndOptional.value() - wadSerializer.GetDirectory().data();
  }

  const auto it = std::ranges::find_if(lumps, [&](const LumpData &lump) {
    return lump.entry.name == name;
  });

  LumpData wallLump{};
  wallLump.entry.name = name;

  std::vector<uint8_t> _binData;
  VectorWriter writer{ _binData };
  writer.WriteRaw(static_cast<uint16_t>(this->width));
  writer.WriteRaw(static_cast<uint16_t>(this->height));

  writer.WriteRaw(this->leftOffset);
  writer.WriteRaw(this->topOffset);

  writer.WriteVector(this->data);

  wallLump.rawData = std::move(_binData);

  if (it == lumps.end()) {
    lumps.insert(lumps.begin() + index, std::move(wallLump));
  } else {
    *it = std::move(wallLump);
  }

  wadSerializer.SetLumps(std::move(lumps));
  wadSerializer.Write();
}

void PatchView::Read()
{
  WadSerializer wadSerializer(DATA_PATH);

  if (!wadSerializer.Load(false)) {
    throw std::runtime_error("Couldn't read a data file while reading a wall texture.");
  }

  const std::optional<directoryEntry *> &directoryEntry = wadSerializer.FindDirectoryEntry(this->name);

  if (!directoryEntry.has_value()) {
    throw std::runtime_error(std::format("Couldn't find directory entry with name: {}", (char *)this->name.data()));
  }

  LumpData lumpData = wadSerializer.ReadLump(*directoryEntry.value());

  constexpr size_t kPatchHeaderSize = (2 * sizeof(uint16_t)) + (2 * sizeof(int16_t));
  if (lumpData.rawData.size() < kPatchHeaderSize) {
    throw std::runtime_error("Patch lump is too small to contain a valid header.");
  }

  size_t index = 0;
  // width and height are stored as uint16_t
  uint16_t width, height;
  memcpy(&width, lumpData.rawData.data(), sizeof(uint16_t));
  index += sizeof(uint16_t);

  memcpy(&height, lumpData.rawData.data() + index, sizeof(uint16_t));
  index += sizeof(uint16_t);

  int16_t _leftOffset, _topOffset;
  memcpy(&_leftOffset, lumpData.rawData.data() + index, sizeof(int16_t));
  index += sizeof(int16_t);

  memcpy(&_topOffset, lumpData.rawData.data() + index, sizeof(int16_t));
  index += sizeof(int16_t);

  this->width = width;
  this->height = height;
  this->leftOffset = _leftOffset;
  this->topOffset = _topOffset;

  // move data from the current index to the end of the lump data into this texture's data vector
  const std::vector<uint8_t> texData(std::make_move_iterator(lumpData.rawData.begin() + index),
                                     std::make_move_iterator(lumpData.rawData.end()));

  VectorReader reader(texData);
  reader.ReadVector(this->data);
}
std::vector<PatchView> PatchView::ReadAll()
{
  WadSerializer wadSerializer(DATA_PATH);

  if (!wadSerializer.Load(false)) {
    return {};
  }
  const lumpName pStartName = WadSerializer::MakeLumpName("P_START");

  const std::optional<directoryEntry *> pStartOptional = wadSerializer.FindDirectoryEntry(pStartName);
  if (!pStartOptional.has_value()) {
    return {};
  }

  const lumpName pEndName = WadSerializer::MakeLumpName("P_END");
  const std::optional<directoryEntry *> &pEndOptional = wadSerializer.FindDirectoryEntry(pEndName);

  std::vector<PatchView> res;
  res.reserve(pEndOptional.value() - pStartOptional.value());

  directoryEntry *curr = pStartOptional.value() + 1;

  while (curr != pEndOptional.value()) {
    PatchView &wallTexture = res.emplace_back(curr->name);
    wallTexture.Read();
    curr++;
  }

  return std::move(res);
}

void WallTexture::Write()
{
  WadSerializer wadSerializer(DATA_PATH);

  if (!wadSerializer.Load(true)) {
    throw std::runtime_error("Couldn't read a data file while writing a wall texture.");
  }
}

void WallTexture::Read()
{}

void TextureManager::LoadFlatTexture(const std::filesystem::path &path, FlatTexture &outFlatTexture) const
{
  assert(palManager);
  const RawImage<uint8_t> &rawImage = image::LoadRawImage<uint8_t>(path, *palManager);
  std::vector<uint8_t> rawData = image::ConvertRawToFlat<uint8_t>(rawImage);

  outFlatTexture.data = std::move(rawData);
  outFlatTexture.width = outFlatTexture.height = FLAT_TEXTURE_SIZE;
}

void TextureManager::LoadWallTexture(const std::filesystem::path &path, PatchView &outWallTexture) const
{
  assert(palManager);
  const RawImage<int16_t> &rawImage = image::LoadRawImage<int16_t>(path, *palManager);
  Patch patch = image::ConvertRawToWall(rawImage);

  outWallTexture.data = std::move(patch.posts);
  outWallTexture.width = patch.width;
  outWallTexture.height = patch.height;
}

void TextureManager::ConvertFlatToWall(FlatTexture &texture)
{
  const auto &it = std::ranges::find_if(flatTextures, [&texture](const FlatTexture &ft) {
    return ft.name == texture.name;
  });
  if (it == flatTextures.end()) {
    std::cerr << "Texture: " << (char *)texture.name.data() << " wasn't found\n";
    return;
  }

  PatchView wallTexture{ texture.name };
  const RawImage<uint8_t> rawImage{ texture.width, texture.height, std::move(texture.data) };
  wallTexture.data = std::move(image::ConvertRawToWall<uint8_t>(rawImage).posts);

  wallTexture.width = texture.width;
  wallTexture.height = texture.height;

  patches.emplace_back(wallTexture);
  flatTextures.erase(it);
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
  flatTextures.emplace_back(WadSerializer::MakeLumpName(std::format(DEFAULT_FLAT_TEX_FORMAT, flatTextures.size() + 1)));
}
void TextureManager::AddDefaultPatch()
{
  patches.emplace_back(WadSerializer::MakeLumpName(std::format(DEFAULT_PATCH_FORMAT, patches.size() + 1)));
}

void TextureManager::AddDefaultWallTexture()
{
  wallTextures.emplace_back(WadSerializer::MakeLumpName(std::format(DEFAULT_WALL_TEXTURE_FORMAT, wallTextures.size() + 1)));
}

std::string_view TextureManager::GetPatchTextureName(const int16_t patchIndex)
{
  if (patchIndex < 0 || patchIndex >= static_cast<int16_t>(patches.size())) {
    return "###";
  }

  return std::string_view((char *)patches[patchIndex].name.data());
}
