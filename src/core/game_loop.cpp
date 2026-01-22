#include "game_loop.h"
#include <iostream>

struct InputState;

namespace gameloop
{

    void handleMouseMovement(SDL_Event &event, InputState &input)
    {
        input.mouse_dx = event.motion.xrel;
        input.mouse_dy = event.motion.yrel;
    }

    void handleKeyInput(SDL_Event &event, InputState &input)
    {
        bool pressed = (event.type == SDL_KEYDOWN);
        SDL_Scancode scancode = event.key.keysym.scancode;
        input.keys[scancode] = pressed;
    }
}
