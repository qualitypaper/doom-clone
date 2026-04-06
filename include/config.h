#pragma once

#include "core/fixed_math.h"


#include <cstdint>
#include <string_view>

constexpr uint16_t WINDOW_WIDTH = 1200;
constexpr uint16_t WINDOW_HEIGHT = 800;

constexpr uint16_t EDITOR_WINDOW_WIDTH = 1500;
constexpr uint16_t EDITOR_WINDOW_HEIGHT = 1200;

constexpr uint16_t CANVAS_WIDTH = 800;
constexpr uint16_t CANVAS_HEIGHT = 600;

constexpr fixed_t CANVAS_WIDTH_FRAC = CANVAS_WIDTH << FRAC_BITS;
constexpr fixed_t CANVAS_CENTERX_FRAC = CANVAS_WIDTH << (FRAC_BITS - 1);
constexpr fixed_t CANVAS_CENTERY_FRAC = CANVAS_HEIGHT << (FRAC_BITS - 1);

constexpr uint8_t DESIRED_FRAMERATE = 144;

constexpr double MOUSE_SENSITIVITY = 1;

inline const std::string_view SAVED_LEVEL_PATH = PROJECT_ROOT_PATH "/levels.wad";
