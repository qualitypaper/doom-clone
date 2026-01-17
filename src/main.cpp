
#include "config.h"
#include "sdl_window.h"

int main()
{
    if (!sdl_window::init())
        return 1;

    constexpr uint32_t n = config::WINDOW_WIDTH * config::WINDOW_HEIGHT;
    static uint32_t pixels[n];


    for (uint32_t i = 0; i < n; ++i)
    {
        uint8_t r = 255, b = 255, g = 0, alpha = 255;

        pixels[i] = (r << 24) | (b << 16) | (g << 8) | alpha;
    }

    sdl_window::updatePixels(pixels);
    sdl_window::updateScreen();

    system("sleep 5");

    sdl_window::kill();

    return 0;
}
