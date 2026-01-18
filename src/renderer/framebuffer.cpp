#include "framebuffer.h"

namespace framebuffer
{
    bool isOutOfBounds(uint16_t y)
    {
        return (y < 0 || y >= config::WINDOW_HEIGHT);
    }

    void FrameBuffer::drawVerticalLine(uint16_t x, uint16_t y0, uint16_t y1, uint32_t color)
    {
        // scale the point to the window size
        uint16_t scaledX = config::SCALE_X * x;
        uint16_t transformedY0 = config::SCALE_Y * y0;
        uint16_t transformedY1 = config::SCALE_Y * y1;

        if (scaledX < 0 || scaledX > config::WINDOW_WIDTH || isOutOfBounds(transformedY0) || isOutOfBounds(transformedY1))
        {
            return;
        }

        // using window width as the pitch, because the current
        // implementation doesn't leave any extra pixels
        uint16_t pitch = config::WINDOW_WIDTH;

        for (uint16_t i = 0; i <= config::SCALE_X + 1; i++)
        {
            uint32_t *ptr = this->pixels + (transformedY0)*pitch + (scaledX + i);

            for (uint16_t y = transformedY0; y <= transformedY1; y++)
            {
                *(uint32_t *)ptr = color;
                ptr += pitch;
            }
        }
    }

    void FrameBuffer::update()
    {
        sdl_window::updatePixels(this->pixels);
        sdl_window::updateScreen();
    }

    void FrameBuffer::pollEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT: 
                    // destroy
                    break;
                case SDL_KEYDOWN:
                    
                    break;
            }
        }
    }
}