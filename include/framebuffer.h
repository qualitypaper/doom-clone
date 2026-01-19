#pragma once

#include <iostream>

#include <glm/glm.hpp>

#include "sdl_window.h"
#include "config.h"

namespace framebuffer
{

    class FrameBuffer
    {
    private:
        uint32_t *pixels;

    public:
        FrameBuffer()
        {
            uint32_t n = config::WINDOW_WIDTH * config::WINDOW_HEIGHT;
            this->pixels = new uint32_t[n];

            for (uint32_t i = 0; i < n; i++)
            {
                this->pixels[i] = 0xFFFFFFFF;
            }

            if (sdl_window::init())
            {
                sdl_window::updatePixels(this->pixels);
                sdl_window::updateScreen();
            }
        }
        ~FrameBuffer()
        {
            delete[] pixels;
            sdl_window::kill();
        }
        
        void drawHorizontalLine(int16_t y, int16_t x0, int16_t x1, uint32_t color);
        void drawVerticalLine(int16_t x, int16_t y0, int16_t y1, uint32_t color);
        void update();
        void pollEvents();
    };
}