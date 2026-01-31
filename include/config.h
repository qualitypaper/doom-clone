#pragma once

#include <cstdint>

namespace config {
constexpr uint16_t WINDOW_WIDTH = 800;
constexpr uint16_t WINDOW_HEIGHT = 600;

constexpr uint16_t EDITOR_WINDOW_WIDTH = 1200;
constexpr uint16_t EDITOR_WINDOW_HEIGHT = 900;

constexpr uint16_t CANVAS_WIDTH = 320;
constexpr uint16_t CANVAS_HEIGHT = 200;

constexpr float SCALE_X = (float)WINDOW_WIDTH / (float)CANVAS_WIDTH;
constexpr float SCALE_Y = (float)WINDOW_HEIGHT / (float)CANVAS_HEIGHT;

constexpr float NEAR_CLIPPING = 1;

constexpr uint16_t VIEWPORT_WIDTH = CANVAS_WIDTH;
constexpr uint16_t VIEWPORT_HEIGHT = CANVAS_HEIGHT;
constexpr const uint32_t CAMERA_DEPTH = 1;

constexpr uint8_t DESIRED_FRAMERATE = 60;

// made for fov of 90 degress, where tan(FOV/2) equals 1, if desired to change add (...)/tan(FOV/2)
constexpr const uint32_t PROJECTION_PLANE_DISTANCE = 1;

constexpr const float MOUSE_SENSITIVITY = 0.01;

constexpr uint8_t FOV = 90;
}// namespace config
