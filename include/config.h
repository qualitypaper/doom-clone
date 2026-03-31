#pragma once

#include <cstdint>

namespace config {
constexpr uint16_t WINDOW_WIDTH = 1200;
constexpr uint16_t WINDOW_HEIGHT = 900;

constexpr uint16_t EDITOR_WINDOW_WIDTH = 1500;
constexpr uint16_t EDITOR_WINDOW_HEIGHT = 1200;

constexpr uint16_t CANVAS_WIDTH = 800;
constexpr uint16_t CANVAS_HEIGHT = 600;

constexpr double SCALE_X = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(CANVAS_WIDTH);
constexpr double SCALE_Y = static_cast<float>(WINDOW_HEIGHT) / static_cast<float>(CANVAS_HEIGHT);

constexpr float NEAR_CLIPPING = 1;

constexpr uint16_t VIEWPORT_WIDTH = CANVAS_WIDTH;
constexpr uint16_t VIEWPORT_HEIGHT = CANVAS_HEIGHT;
constexpr uint32_t CAMERA_DEPTH = 1;

constexpr uint8_t DESIRED_FRAMERATE = 144;

// made for fov of 90 degress, where tan(FOV/2) equals 1, if desired to change add (...)/tan(FOV/2)
constexpr uint32_t PROJECTION_PLANE_DISTANCE = 1;

constexpr float MOUSE_SENSITIVITY = 0.01f;

constexpr const char *SAVED_LEVEL_PATH = "levels.wad";
}// namespace config
