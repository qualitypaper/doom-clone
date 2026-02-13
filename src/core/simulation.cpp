#include "simulation.h"
#include "config.h"

namespace simulation {

// updates the gamec state with a constant rate of @param dt
void update(GameState &gameState, const InputState &input, const double_t dt)
{

  // mouse
  const double_t angleDiff = std::atan(config::MOUSE_SENSITIVITY * input.mouse_dx / config::PROJECTION_PLANE_DISTANCE);
  gameState.playerState.angle += angleDiff;

  double_t &x = gameState.playerState.x, &y = gameState.playerState.y;

  const double_t sin = std::sin(gameState.playerState.angle);
  const double_t cos = std::cos(gameState.playerState.angle);

  int8_t moveSide = 0, moveForward = 0;

  if (input.keys[SDL_SCANCODE_W]) moveForward += 1;
  if (input.keys[SDL_SCANCODE_S]) moveForward -= 1;
  if (input.keys[SDL_SCANCODE_A]) moveSide -= 1;
  if (input.keys[SDL_SCANCODE_D]) moveSide += 1;

  const double_t velocity = gameState.playerState.velocity;

  x += (moveSide * cos + moveForward * sin) * dt * velocity;
  y += (moveSide * (-sin) + moveForward * cos) * dt * velocity;
}

}// namespace simulation