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

  const std::optional<directoryEntry *> &fStartOptional = wadSerializer.FindEntry(fStartName);

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
    const directoryEntry *fEndOptional = wadSerializer.FindEntry(fEndName);

    fEndIndex = fEndOptional - directory;
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

  const std::optional<directoryEntry *> &entry = wadSerializer.FindEntry(name);

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

  const std::optional<directoryEntry *> fStartOptional = wadSerializer.FindEntry(fStartName);
  if (!fStartOptional.has_value()) {
    return {};
  }

  const lumpName fEndName = WadSerializer::MakeLumpName("F_END");
  const std::optional<directoryEntry *> &fEndOptional = wadSerializer.FindEntry(fEndName);

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
    DOOM_CORE_ERROR("Couldn't load the data file while reading a wall texture.");
    return;
  }

  std::vector<LumpData> &lumps = wadSerializer.GetLoadedLumps();

  size_t index = 0;
  // new texture
  const directoryEntry *pStartPtr = wadSerializer.FindEntry(WadSerializer::MakeLumpName("P_START"));
  const directoryEntry *pEndPtr = wadSerializer.FindEntry(WadSerializer::MakeLumpName("P_END"));

  DOOM_CORE_ASSERT(!(!pStartPtr ^ !pEndPtr),
                   "Only one of P_START and P_END entries is present in the WAD file, which is not expected.");

  if (!pStartPtr && !pEndPtr) {
    std::vector<directoryEntry> &directory = wadSerializer.GetDirectory();

    directoryEntry &pStart = directory.emplace_back(0, 0, WadSerializer::MakeLumpName("P_START"));
    directoryEntry &pEnd = directory.emplace_back(0, 0, WadSerializer::MakeLumpName("P_END"));

    lumps.emplace_back(std::vector<uint8_t>(), pStart);
    lumps.emplace_back(std::vector<uint8_t>(), pEnd);

    index = lumps.size() - 1;
  } else {
    index = pEndPtr - wadSerializer.GetDirectory().data();
  }

  const auto it = std::ranges::find_if(lumps, [&](const LumpData &lump) {
    return lump.entry.name == name;
  });

  LumpData wallLump{};
  wallLump.entry.name = name;

  std::vector<uint8_t> _binData;
  _binData.reserve(2 * sizeof(uint16_t) + 2 * sizeof(int16_t) + data.size() * sizeof(Post));
  VectorWriter writer{ _binData };
  writer.WriteRaw(static_cast<uint16_t>(this->width));
  writer.WriteRaw(static_cast<uint16_t>(this->height));

  writer.WriteRaw(this->leftOffset);
  writer.WriteRaw(this->topOffset);

  writer.WriteVector(this->data);

  _binData.shrink_to_fit();
  wallLump.rawData = std::move(_binData);

  if (it == lumps.end()) {
    lumps.insert(lumps.begin() + index, std::move(wallLump));
  } else {
    *it = std::move(wallLump);
  }

  wadSerializer.Write();
}

void PatchView::Read()
{
  WadSerializer wadSerializer(DATA_PATH);

  if (!wadSerializer.Load(false)) {
    throw std::runtime_error("Couldn't read a data file while reading a wall texture.");
  }

  const std::optional<directoryEntry *> &directoryEntry = wadSerializer.FindEntry(this->name);

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

  directoryEntry *pStart = wadSerializer.FindEntry(pStartName);
  if (!pStart) {
    return {};
  }

  const lumpName pEndName = WadSerializer::MakeLumpName("P_END");
  const directoryEntry *pEnd = wadSerializer.FindEntry(pEndName);

  std::vector<PatchView> res;
  res.reserve(pEnd - pStart);

  directoryEntry *curr = pStart + 1;

  while (curr != pEnd) {
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
{
  WadSerializer wadSerializer(DATA_PATH);

  if (!wadSerializer.Load(false)) {
    throw std::runtime_error("Couldn't read a data file while reading a wall texture.");
  }

  const std::optional<directoryEntry *> &directoryEntry = wadSerializer.FindEntry(this->mapTexture.name);

  if (!directoryEntry.has_value()) {
    throw std::runtime_error(
      std::format("Couldn't find directory entry with name: {}", (char *)this->mapTexture.name.data()));
  }

  const LumpData lumpData = std::move(wadSerializer.ReadLump(*directoryEntry.value()));
  VectorReader reader(lumpData.rawData);
  this->mapTexture.deserialize(reader);
}

std::vector<WallTexture> WallTexture::ReadAll()
{
  WadSerializer wadSerializer(DATA_PATH);

  if (!wadSerializer.Load(false)) {
    return {};
  }
  const lumpName texture1Name = WadSerializer::MakeLumpName("TEXTURE1");

  const directoryEntry *texture1 = wadSerializer.FindEntry(texture1Name);
  if (!texture1) {
    return {};
  }

  const LumpData lumpData = std::move(wadSerializer.ReadLump(*texture1));

  VectorReader reader(lumpData.rawData);

  int32_t numTextures;
  reader.ReadRaw(numTextures);

  std::vector<MapTexture> textures;
  textures.reserve(numTextures);

  reader.ReadVector(textures, false);

  std::vector<WallTexture> _wallTextures;
  _wallTextures.reserve(static_cast<size_t>(numTextures));

  for (auto &el : textures) {
    _wallTextures.emplace_back(el);
  }

  return std::move(_wallTextures);
}

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
  const auto &it = std::ranges::find_if(m_flatTextures, [&texture](const FlatTexture &ft) {
    return ft.name == texture.name;
  });
  if (it == m_flatTextures.end()) {
    std::cerr << "Texture: " << (char *)texture.name.data() << " wasn't found\n";
    return;
  }

  PatchView wallTexture{ texture.name };
  const RawImage<uint8_t> rawImage{ texture.width, texture.height, std::move(texture.data) };
  wallTexture.data = std::move(image::ConvertRawToWall<uint8_t>(rawImage).posts);

  wallTexture.width = texture.width;
  wallTexture.height = texture.height;

  m_patches.emplace_back(wallTexture);
  m_flatTextures.erase(it);
}

const FlatTexture *TextureManager::GetFlatTexture(const size_t index) const
{
  if (index >= m_flatTextures.size())
    return {};

  return &m_flatTextures.at(index);
}

std::string TextureManager::GetFlatTextureName(const size_t index) const
{
  const FlatTexture *flatTexture = GetFlatTexture(index);
  std::string textureName;

  if (!flatTexture) {
    textureName = "###";
  } else {
    textureName = (char *)flatTexture->name.data();
  }

  return std::move(textureName);
}
const WallTexture *TextureManager::GetWallTexture(const size_t index) const
{
  if (index >= m_wallTextures.size() ) return nullptr;

  return &m_wallTextures[index];
}
const PatchView *TextureManager::GetPatch(const size_t index) const
{
  if (index >= m_patches.size() ) return nullptr;

  return &m_patches[index];
}

void TextureManager::AddDefaultFlatTexture()
{
  m_flatTextures.emplace_back(
    WadSerializer::MakeLumpName(std::format(DEFAULT_FLAT_TEX_FORMAT, m_flatTextures.size() + 1)));
}
void TextureManager::AddDefaultPatch()
{
  m_patches.emplace_back(WadSerializer::MakeLumpName(std::format(DEFAULT_PATCH_FORMAT, m_patches.size() + 1)));
}

void TextureManager::AddDefaultWallTexture()
{
  m_wallTextures.emplace_back(
    WadSerializer::MakeLumpName(std::format(DEFAULT_WALL_TEXTURE_FORMAT, m_wallTextures.size() + 1)));
}

std::string_view TextureManager::GetPatchTextureName(const int16_t patchIndex)
{
  if (patchIndex < 0 || patchIndex >= static_cast<int16_t>(m_patches.size())) {
    return "###";
  }

  return { (char *)m_patches[patchIndex].name.data() };
}

void TextureManager::WritePNames(WadSerializer &wadSerializer) const
{
  std::vector<LumpData> &lumps = wadSerializer.GetLoadedLumps();

  // serialize pnames data
  std::vector<uint8_t> pNamesData;
  VectorWriter writer{ pNamesData };

  for (const PatchView &patch : m_patches) {
    writer.WriteRaw(patch.name);
  }

  const lumpName &pNamesLumpName = WadSerializer::MakeLumpName("PNAMES");
  const directoryEntry *pNames = wadSerializer.FindEntry(pNamesLumpName);

  if (!pNames) {
    // create pnames entry if it doesn't exist
    const directoryEntry *entryAfterLevels = wadSerializer.FindEntryAfterLevels();

    const size_t insertIndex =
      entryAfterLevels ? entryAfterLevels - wadSerializer.GetDirectory().data() : lumps.size() - 1;

    lumps.emplace(lumps.begin() + insertIndex, std::move(pNamesData), directoryEntry{ 0, 0, pNamesLumpName });

    wadSerializer.Write();
    return;
  }

  // existing pnames entry
  const long i = pNames - wadSerializer.GetDirectory().data();

  lumps[i] = LumpData{ std::move(pNamesData), *pNames };

  wadSerializer.Write();
}

void TextureManager::WriteWallTexture(const WallTexture &value) const
{
  WadSerializer wadSerializer(DATA_PATH);

  if (!wadSerializer.Load(true)) {
    throw std::runtime_error("Couldn't read a data file while saving a wall texture.");
  }

  if (m_prevPatchesSize != this->m_patches.size()) {
    // write updated pnames
    this->WritePNames(wadSerializer);
  }

  std::vector<LumpData> &lumps = wadSerializer.GetLoadedLumps();

  const MapTexture &mapTexture = value.mapTexture;

  const lumpName texture1Name = WadSerializer::MakeLumpName("TEXTURE1");
  const directoryEntry *entry = wadSerializer.FindEntry(texture1Name);

  if (!entry) {
    // create TEXTURE1 lump
    const directoryEntry *pNames = wadSerializer.FindEntry(WadSerializer::MakeLumpName("PNAMES"));
    // *pNames is never null
    const size_t insertIndex = pNames - wadSerializer.GetDirectory().data();
    // TEXTURE1 must come after PNAMES

    std::vector<uint8_t> buffer;
    buffer.reserve(sizeof(int32_t) + sizeof(MapTexture) + mapTexture.patches.size() * sizeof(MapPatch));
    VectorWriter writer{ buffer };
    writer.WriteRaw(1);// number of textures
    mapTexture.serialize(writer);

    lumps.emplace(lumps.begin() + insertIndex + 1, std::move(buffer), directoryEntry{ 0, 0, texture1Name });
    wadSerializer.Write();
    return;
  }

  // update existing TEXTURE1 lump
  const long currIndex = entry - wadSerializer.GetDirectory().data();

  const LumpData *texture1Lump = wadSerializer.FindLoadedLump(texture1Name);

  DOOM_CORE_ASSERT(texture1Lump, "TEXTURE1 lump was not loaded.");

  int32_t numTextures = 0;
  std::vector<MapTexture> textures;
  bool found = false;

  {
    VectorReader reader(texture1Lump->rawData);
    reader.ReadRaw(numTextures);

    textures.reserve(numTextures);
    for (int i = 0; i < numTextures; i++) {
      MapTexture texture;
      texture.deserialize(reader);

      if (texture.name == value.mapTexture.name) {
        texture = value.mapTexture;
        found = true;
      }

      textures.push_back(std::move(texture));
    }
  }

  if (!found) {
    textures.push_back(value.mapTexture);
    numTextures++;
  }

  std::vector<uint8_t> buffer;
  buffer.reserve(sizeof(int32_t)
                 + textures.size() * (sizeof(MapTexture) + textures[0].patches.size() * sizeof(MapPatch)));
  VectorWriter writer{ buffer };
  writer.WriteRaw(numTextures);
  writer.WriteVector(textures, false);

  lumps[currIndex] = LumpData{ std::move(buffer), *entry };
  wadSerializer.Write();
}
