#include "main.h"
#include "SDL.h"

#define WIDTH 640
#define HEIGHT 480

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Surface *winSurface;
SDL_Texture *texture;

namespace render
{

    void updatePixels(uint32_t *pixels)
    {
        SDL_UpdateTexture(texture, NULL, pixels, WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    bool init()
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        {
            std::cout << "SDL failed to initialize, Error: " << SDL_GetError() << '\n';
            return false;
        }

        window = SDL_CreateWindow("Doom Clone", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);

        if (!window)
        {
            std::cout << "SDL failed to create a window, Error: " << SDL_GetError() << '\n';
            return false;
        }

        winSurface = SDL_GetWindowSurface(window);

        if (!winSurface)
        {
            std::cout << "SDL failed to create a window surface, Error: " << SDL_GetError() << '\n';
            return false;
        }

        renderer = SDL_CreateSoftwareRenderer(winSurface);
        if (!renderer)
        {
            std::cout << "SDL failed to create a software renderer, Error: " << SDL_GetError() << '\n';
            return false;
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, winSurface->w, winSurface->h);

        if (!texture)
        {
            std::cout << "SDL failed to create a texture, Error: " << SDL_GetError() << '\n';
            return false;
        }

        return true;

        return true;
    }

    void kill()
    {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);

        SDL_DestroyWindow(window);
        SDL_Quit();
    }
}




int main()
{
    if (!render::init())
        return 1;

    static uint32_t pixels[WIDTH * HEIGHT];

    for (int i = 0; i < WIDTH * HEIGHT; ++i)
    {
        uint8_t r = 255, b = 255, g = 0, alpha = 255;

        pixels[i] = (r << 24) | (b << 16) | (g << 8) | alpha;
    }

    render::updatePixels(pixels);

    SDL_UpdateWindowSurface(window);

    system("sleep 5");

    render::kill();

    return 0;
}
