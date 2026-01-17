#include "framebuffer.h"

namespace framebuffer
{
    void FrameBuffer::color(glm::vec2 point, uint32_t color)
    {
        // scale the point to the window size
        glm::vec2 scaledPoint = glm::vec2{config::SCALE_X * point.x, config::SCALE_Y * point.y};

        int x = static_cast<int>(scaledPoint.x);
        int y = static_cast<int>(scaledPoint.y);
        
        // Bounds check
        if (x < 0 || x >= config::WINDOW_WIDTH || y < 0 || y >= config::WINDOW_HEIGHT) {
            return;
        }

        uint32_t index = y * config::WINDOW_WIDTH + x;
        this->pixels[index] = color;
    }

    void FrameBuffer::update()
    {
        sdl_window::updatePixels(this->pixels);
        sdl_window::updateScreen();
    }
}