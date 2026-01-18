#pragma once

#include <SDL.h>

struct InputState {
    bool keys[SDL_NUM_SCANCODES];
    bool mouse_buttons[8];
    int mouse_dx;
    int mouse_dy;
};

namespace sdl_window
{
    bool init();
    void kill();
    void updatePixels(uint32_t* pixels);
    void updateScreen();
}