#pragma once

#include <cstdint>

namespace config
{
    constexpr uint16_t WINDOW_WIDTH = 800;
    constexpr uint16_t WINDOW_HEIGHT = 600;

    constexpr uint16_t CANVAS_WIDTH = 320;
    constexpr uint16_t CANVAS_HEIGHT = 200;

    constexpr float SCALE_X = WINDOW_WIDTH / CANVAS_WIDTH;
    constexpr float SCALE_Y = WINDOW_HEIGHT / CANVAS_HEIGHT;

    constexpr uint8_t VIEWPORT_WIDTH = 1;
    constexpr uint8_t VIEWPORT_HEIGHT = 1;

    constexpr uint32_t CAMERA_DEPTH = 1;

    // made for fov of 90 degress, where tan(FOV/2) equals 1, if desired to change add (...)/tan(FOV/2)
    constexpr uint32_t PROJECTION_PLANE_DISTANCE = (CANVAS_WIDTH/ 2);

    constexpr uint8_t FOV = 90;
}