#include "framebuffer.h"
#include "game_loop.h"

#include <iostream>

void poll_sdl_events(InputState &input);
void update(const game_loop::GameState &gameState);
void render(const game_loop::GameState &gameState, InputState &input);

uint32_t mapColor(uint8_t r, uint8_t g, uint8_t b, uint8_t alpha)
{
    return (r << 24) | (g << 16) | (b << 8) | alpha;
}

framebuffer::FrameBuffer *fb;
bool running;

int main()
{
    fb = new framebuffer::FrameBuffer();

    SDL_SetRelativeMouseMode(SDL_TRUE);

    std::cout << "Set up window" << '\n';

    InputState input{};
    game_loop::GameState gameState{};

    running = true;
    // game loop
    time_t timestamp = time(0);

    std::cout << "Start timestamp: " << timestamp << '\n';

    while (running)
    {
        input.mouse_dx = 0;
        input.mouse_dy = 0;

        poll_sdl_events(input);

        update(gameState);
        render(gameState, input);
        system("sleep 1");

        if (time(0) - timestamp > 5000)
        {
            running = false;
        }
    }

    if (fb)
    {
        delete fb;
    }

    return 0;
}

void poll_sdl_events(InputState &input)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        std::cout << "Event type: " << event.type << '\n';
        if (event.type == 768)
        {
            std::printf("Pressed a key \n");
        }
        switch (event.type)
        {
        SDL_QUIT:
            running = false;
            break;
        SDL_KEYDOWN:
        SDL_KEYUP:
        {
            std::cout << "Pressed" << '\n';
            bool pressed = (event.type == SDL_KEYDOWN);
            SDL_Scancode scancode = event.key.keysym.scancode;
            input.keys[scancode] = pressed;
            std::cout << "Pressed: " << event.key.keysym.sym << '\n';
            break;
        }
        SDL_MOUSEMOTION:
            input.mouse_dx = event.motion.xrel;
            input.mouse_dy = event.motion.yrel;
            break;
        }
    }
    std::printf("Finished polling events \n");
}

void update(const game_loop::GameState &gameState)
{
}

void render(const game_loop::GameState &gaeState, InputState &input)
{
    uint32_t yellow = mapColor(255, 255, 0, 255);

    for (int i = 0; i < 50; i++)
    {
        fb->drawVerticalLine(i, 0, 50, yellow);
    }

    fb->update();
}