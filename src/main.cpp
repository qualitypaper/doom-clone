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
        .floorHeight = 10,
        .ceilingHeight = 256,
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
        .x = 50,
        .y = 250,
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
        // std::cout << "Event type: " << event.type << '\n';

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
        auto &sector = sectors[sidedef.sectorId];

        // apply translation and rotation depending on the player's position
        int16_t startX = vertices[ld.start].x - gameState.playerState.x;
        int16_t startY = vertices[ld.start].y - gameState.playerState.y;

        int16_t endX = vertices[ld.end].x - gameState.playerState.x;
        int16_t endY = vertices[ld.end].y - gameState.playerState.y;

        // Calculate floor and ceiling heights relative to player
        int16_t floorZ = sector.floorHeight - gameState.playerState.z;
        int16_t ceilingZ = sector.ceilingHeight - gameState.playerState.z;

        int16_t viewX1 = startX * std::cos(gameState.playerState.angle) - startY * std::sin(gameState.playerState.angle);
        int16_t viewY1 = startX * std::sin(gameState.playerState.angle) + startY * std::cos(gameState.playerState.angle);
        int16_t viewX2 = endX * std::cos(gameState.playerState.angle) - endY * std::sin(gameState.playerState.angle);
        int16_t viewY2 = endX * std::sin(gameState.playerState.angle) + endY * std::cos(gameState.playerState.angle);

        // near plane clipping
        // if (viewY1 < 1 || viewY2 < 1) continue;

        int16_t projectedStartX = config::CANVAS_WIDTH/2 + viewX1 / viewY1;
        int16_t projectedEndX = config::CANVAS_WIDTH/2 + viewX2 / viewY2;

        int16_t start = std::min(projectedStartX, projectedEndX);
        int16_t end = std::max(projectedStartX, projectedEndX);

        for (int i = start; i <= end; i++)
        {
            double_t inv_y1 = 1 / (double) viewY1;
            double_t inv_y2 = 1 / (double) viewY2;
            double_t t = double(i - start) / double(end - start);
            double_t inv_y = lerp(inv_y1, inv_y2, t);
            int16_t projectedFloorZ = config::CANVAS_HEIGHT/2 - floorZ * inv_y;
            int16_t projectedCeilingZ = config::CANVAS_HEIGHT/2 - ceilingZ * inv_y;

            std::cout << "top: " << projectedCeilingZ << ", bottom: " << projectedFloorZ << '\n';

            fb->drawVerticalLine(i, projectedCeilingZ, projectedFloorZ, mapColor(0, 255, 0, 255));
        }
    }

    fb->update();
}