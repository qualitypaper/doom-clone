#pragma once

#include "core/sdl_window.h"
#include "defs.h"

#include <algorithm>
#include <array>
#include <format>
#include <memory>
#include <string>
#include <vector>


class EditorRenderer;
class Renderer;
/*
 X, Y - horizontal planes, X - east-west, Y - north-south
 Z - vertical plane,
*/
enum class EngineMode { GAMEPLAY_3D, EDITOR_2D, BSP_VIEWER };

struct Player
{
  fixed_t x{};
  fixed_t y{};
  fixed_t z{};
  fixed_t velocity{};
  angle_t angle{};
};

struct Level
{
  explicit Level(const std::array<char8_t, 8> _name) : name(_name) {}

  std::vector<Vertex> vertices{};
  std::vector<line_t> linedefs{};
  std::vector<side_t> sidedefs{};
  std::vector<Sector> sectors{};
  std::vector<Node> nodes{};
  std::vector<SubSector> subsectors{};
  std::vector<seg_t> segments{};
  std::array<char8_t, 8> name;

  void Load(FileReader &fr);

  static void Init(Level &level,
                   const std::vector<LineDef> &_lines,
                   const std::vector<SideDef> &_sides,
                   const std::vector<Seg> &_segs);
};

struct GameState
{
  Player player{};
  EngineMode currentMode;
  uint16_t levelNum{};
  std::shared_ptr<SdlWindow> sdlWindow;
  std::shared_ptr<Renderer> renderer;
  std::shared_ptr<EditorRenderer> editorRenderer;
  InputState input;

  void Reset();
};

void HandleMouseMovement(const SDL_Event &event, InputState &input);
void HandleKeyInput(const SDL_Event &event, InputState &input);
void SetEngineMode(GameState &gameState, EngineMode newMode, const std::shared_ptr<SdlWindow> &sdlWindow);