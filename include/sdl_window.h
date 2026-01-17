#pragma once

#include <SDL.h>

namespace sdl_window
{
    bool init();
    void kill();
    void updatePixels(uint32_t* pixels);
    void updateScreen();
}