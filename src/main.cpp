#include "framebuffer.h"
#include "game_loop.h"
#include "camera.h"

#include <iostream>
#include <assert.h>

void poll_sdl_events(InputState &input);
void update(const gameloop::GameState &gameState);
void render(const gameloop::GameState &gameState, InputState &input);

constexpr uint32_t mapColor(uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) { return (r << 24) | (g << 16) | (b << 8) | alpha; }

constexpr uint32_t YELLOW = mapColor(255, 255, 0, 255);

framebuffer::FrameBuffer *fb;
bool running;

static const glm::vec2 vertices[] = {
    {0, 0},
    {256, 0},
    {256, 256},
    {0, 256}};

static const gameloop::Sector sectors[] = {
    {
        .floorHeight = 0,
        .ceilingHeight = 128,
        .lightLevel = 192,
    }};

static const gameloop::SideDef sidedefs[] = {
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
};

static const gameloop::LineDef linedefs[] = {
    {0, 1, gameloop::LineDefType::REGULAR, 0, -1},
    {1, 2, gameloop::LineDefType::REGULAR, 1, -1},
    {2, 3, gameloop::LineDefType::REGULAR, 2, -1},
    {3, 1, gameloop::LineDefType::REGULAR, 3, -1},
};

int main()
{
    assert(sectors[0].floorHeight < sectors[0].ceilingHeight);
    for (auto &ld : linedefs)
    {
        assert(ld.start != ld.end);
        assert(ld.frontSidedef >= 0);
    }

    fb = new framebuffer::FrameBuffer();

    // SDL_SetRelativeMouseMode(SDL_TRUE);

    std::cout << "Set up window" << '\n';

    InputState input{};
    gameloop::GameState gameState{};

    gameState.playerState = entity::Player{
        .x = 0,
        .y = 0,
        .z = 0,
        .velocity = 1.0f,
        .angle = 0,
        .health = 100,
        .armor = 100,
        .current_weapon = 0};

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

        switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            gameloop::handleKeyInput(event, input);
            break;
        case SDL_MOUSEMOTION:
            gameloop::handleMouseMovement(event, input);
            break;
        }
    }
    std::printf("Finished polling events \n");
}

void update(const gameloop::GameState &gameState) {}

void render(const gameloop::GameState &gameState, InputState &input)
{

    // for (int i = 0; i < 50; i++)
    // {
    //     fb->drawHorizontalLine(i, 50, 100, YELLOW);
    // }
    // fb->drawVerticalLine(50, 50, 100, YELLOW);

    // fb->update();
    

    for (auto &ld : linedefs)
    {
        auto &sidedef = sidedefs[ld.frontSidedef];

        auto projected1 = camera::project(vertices[ld.start].x, vertices[ld.start].y, 1);
        auto projected2 = camera::project(vertices[ld.end].x, vertices[ld.end].y, 1);

        auto &sector = sectors[sidedef.sectorId];

        int startX, endX;
        int startY = sector.ceilingHeight, endY = sector.floorHeight;

        if (projected1.x > projected2.x)
        {
            startX = projected2.x;
            endX = projected1.x;
        }
        else
        {
            startX = projected1.x;
            endX = projected2.x;
        }

        for (int i = startX; i <= endX; i++)
        {
            fb->drawVerticalLine(i, startY, endY, YELLOW);
        }
    }

    fb->update();
}