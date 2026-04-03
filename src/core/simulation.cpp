#include "simulation.h"
#include "config.h"
#include "renderer/game/renderer.h"
#include "renderer/game/renderer_helper.h"

namespace simulation {

// updates the game state with a constant rate of @param dt
void update(GameState &gameState, const InputState &input, const double_t dt)
{
  // mouse
  const fixed_t mouseMovement = DoubleToFixed((double)input.mouse_dx * config::MOUSE_SENSITIVITY);
  const angle_t angleDiff = (mouseMovement < 0 ? 1 : -ddd1) * tantoangle[SlopeDiv(std::abs(mouseMovement), FOCAL_LENGTH)];
  gameState.playerState.angle = (gameState.playerState.angle + angleDiff);

  fixed_t &x = gameState.playerState.x, &y = gameState.playerState.y;

  const fixed_t sin = finesine[gameState.playerState.angle >> ANGLE_TO_FINE_SHIFT];
  const fixed_t cos = finesine[(gameState.playerState.angle + ANG90) >> ANGLE_TO_FINE_SHIFT];

  fixed_t moveSide = 0, moveForward = 0;

  if (input.keys[SDL_SCANCODE_W])
    moveForward += FRAC_UNIT;
  if (input.keys[SDL_SCANCODE_S])
    moveForward -= FRAC_UNIT;
  if (input.keys[SDL_SCANCODE_A])
    moveSide -= FRAC_UNIT;
  if (input.keys[SDL_SCANCODE_D])
    moveSide += FRAC_UNIT;

  const fixed_t velocity = gameState.playerState.velocity;

  const fixed_t dis = FixedMul(DoubleToFixed(dt), velocity);

  x += FixedMul(FixedMul(moveSide, cos) + FixedMul(moveForward, sin), dis);
  y += FixedMul(FixedMul(moveSide, (-sin)) + FixedMul(moveForward, cos), dis);
}

}// namespace simulation