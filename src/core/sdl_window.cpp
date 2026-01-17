#include <iostream>

#include "sdl_window.h"
#include "config.h"

namespace sdl_window
{
    SDL_Window *window;
    SDL_Surface *surface;
    SDL_Renderer *renderer;
    SDL_Texture *texture;

    void updatePixels(uint32_t* pixels)
    {
        SDL_UpdateTexture(texture, NULL, pixels, config::WINDOW_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    void updateScreen()
    {
        SDL_UpdateWindowSurface(window);
    }

    bool init()
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        {
            std::cout << "SDL failed to initialize, Error: " << SDL_GetError() << '\n';
            return false;
        }

        window = SDL_CreateWindow("Doom Clone", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, config::WINDOW_WIDTH, config::WINDOW_HEIGHT, SDL_WINDOW_SHOWN);

        if (!window)
        {
            std::cout << "SDL failed to create a window, Error: " << SDL_GetError() << '\n';
            return false;
        }

        surface = SDL_GetWindowSurface(window);

        if (!surface)
        {
            std::cout << "SDL failed to create a window surface, Error: " << SDL_GetError() << '\n';
            return false;
        }

        renderer = SDL_CreateSoftwareRenderer(surface);
        if (!renderer)
        {
            std::cout << "SDL failed to create a software renderer, Error: " << SDL_GetError() << '\n';
            return false;
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, config::WINDOW_WIDTH, config::WINDOW_HEIGHT);

        if (!texture)
        {
            std::cout << "SDL failed to create a texture, Error: " << SDL_GetError() << '\n';
            return false;
        }

        return true;
    }

    void kill()
    {
        SDL_FreeSurface(surface);
        SDL_DestroyWindow(window);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyTexture(texture);

        SDL_Quit();
    }

}