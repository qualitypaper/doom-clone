#include "simulation.h"
#include <iostream>

namespace simulation {

// updates the gamec state with a constant rate of @param dt
void update(gameloop::GameState &gameState, InputState &input, const double_t dt)
{

  // mouse
  double_t angleDiff = std::atan(config::MOUSE_SENSITIVITY * input.mouse_dx / config::PROJECTION_PLANE_DISTANCE);
  gameState.playerState.angle += angleDiff;

  float_t &x = gameState.playerState.x, &y = gameState.playerState.y;

  float_t sin = std::sin(gameState.playerState.angle);
  float_t cos = std::cos(gameState.playerState.angle);

  int8_t moveSide = 0, moveForward = 0;

  if (input.keys[SDL_SCANCODE_F1]) {
    // change the game mode
    std::cout << "Changing the game mode" << '\n';
    if (gameState.currentMode == gameloop::ViewMode::GAMEPLAY_3D) {
      gameState.currentMode = gameloop::ViewMode::EDITOR_2D;
    } else {
      gameState.currentMode = gameloop::ViewMode::GAMEPLAY_3D;
    }
  }

  if (input.keys[SDL_SCANCODE_W]) moveForward += 1;
  if (input.keys[SDL_SCANCODE_S]) moveForward -= 1;
  if (input.keys[SDL_SCANCODE_A]) moveSide -= 1;
  if (input.keys[SDL_SCANCODE_D]) moveSide += 1;

  float_t velocity = gameState.playerState.velocity;

  x += (moveSide * cos + moveForward * sin) * dt * velocity;
  y += (moveSide * (-sin) + moveForward * cos) * dt * velocity;
}

}