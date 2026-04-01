#include "gameloop.h"
#include "../bsp/bsp.h"
#include "config.h"
#include "sdl_window.h"

#include <SDL_mouse.h>
#include <SDL_stdinc.h>
#include <SDL_video.h>

struct InputState;

void HandleMouseMovement(const SDL_Event &event, InputState &input)
{
  input.mouse_dx = event.motion.xrel;
  input.mouse_dy = event.motion.yrel;
}

void HandleKeyInput(const SDL_Event &event, InputState &input)
{
  const bool pressed = (event.type == SDL_KEYDOWN);
  const SDL_Scancode scancode = event.key.keysym.scancode;
  input.keys[scancode] = pressed;
}

void SetEngineMode(GameState &state, InputState &input, const EngineMode newMode, const SdlWindow &sdlWindow)
{
  // nothing to change
  if (state.currentMode == newMode)
    return;

  state.currentMode = newMode;

  if (newMode == EngineMode::GAMEPLAY_3D) {
    // disable absolute mouse
    // SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_SetWindowFullscreen(sdlWindow.getWindow(), SDL_FALSE);
    SDL_SetWindowPosition(sdlWindow.getWindow(), SDL_WINDOWPOS_CENTERED_DISPLAY(1), SDL_WINDOWPOS_CENTERED_DISPLAY(1));
    SDL_SetWindowSize(sdlWindow.getWindow(), config::WINDOW_WIDTH, config::WINDOW_HEIGHT);
  } else if (newMode == EngineMode::BSP_VIEWER) {
    // in order to use mouse cursor
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_SetWindowSize(sdlWindow.getWindow(), config::EDITOR_WINDOW_WIDTH, config::EDITOR_WINDOW_HEIGHT);
    // SDL_SetWindowFullscreen(sdlWindow.getWindow(), SDL_TRUE);

    // wiping clean the state, in order to prevent unexpected key and mouse inputs
    memset(input.keys, false, sizeof(input.keys));
    memset(input.mouse_buttons, false, sizeof(input.mouse_buttons));

    input.mouse_dx = 0;
    input.mouse_dy = 0;
  } else {
    // in order to use mouse cursor
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_SetWindowSize(sdlWindow.getWindow(), config::EDITOR_WINDOW_WIDTH, config::EDITOR_WINDOW_HEIGHT);
    // SDL_SetWindowFullscreen(sdlWindow.getWindow(), SDL_TRUE);

    // wiping clean the state, in order to prevent unexpected key and mouse inputs
    memset(input.keys, false, sizeof(input.keys));
    memset(input.mouse_buttons, false, sizeof(input.mouse_buttons));
    input.mouse_dx = 0;
    input.mouse_dy = 0;
  }
}

void Level::Load(FileReader &fr)
{
  if (!fr.IsStreamGood()) {
    throw std::runtime_error("Failed to open file for loading.");
  }

  std::vector<LineDef> _linedefs;
  std::vector<SideDef> _sidedefs;
  std::vector<Seg> segs;

  fr.ReadVector(_linedefs);
  fr.ReadVector(_sidedefs);
  fr.ReadVector(this->vertices);
  fr.ReadVector(segs);
  fr.ReadVector(this->subsectors);
  fr.ReadVector(this->nodes);
  fr.ReadVector(this->sectors);

  Level::Init(*this, _linedefs, _sidedefs, segs);

  // adding sector into each subsector, for easier access during rendering
  for (auto &ssector : subsectors) {
    const seg_t *segment = &segments[ssector.firstSegIndex];

    if (segment->side) {
      ssector.sector = segment->line->backSide->sector;
    } else {
      ssector.sector = segment->line->frontSide->sector;
    }
  }
}
void Level::Init(Level &level,
  const std::vector<LineDef> &_lines,
  const std::vector<SideDef> &_sides,
  const std::vector<Seg> &_segs)
{
  level.linedefs.reserve(_lines.size());
  level.sidedefs.reserve(_sides.size());

  for (const SideDef &side : _sides) {
    level.sidedefs.emplace_back(side.sectorId < 0 ? nullptr : &level.sectors[side.sectorId],
      side.xOffset,
      side.yOffset,
      side.upperWallTexture,
      side.middleWallTexture,
      side.bottomWallTexture);
  }

  for (const LineDef &line : _lines) {
    level.linedefs.emplace_back(&level.vertices[line.start],
      &level.vertices[line.end],
      line.type,
      line.frontSidedef == -1 ? nullptr : &level.sidedefs[line.frontSidedef],
      line.backSidedef == -1 ? nullptr : &level.sidedefs[line.backSidedef]);
  }

  for (const Seg &seg : _segs) {
    level.segments.emplace_back(&level.vertices[seg.startVertex],
      &level.vertices[seg.endVertex],
      seg.angle,
      &level.linedefs[seg.linedefIndex],
      seg.side,
      seg.offset);
  }
}
