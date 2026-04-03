#include "simulation.h"
#include "config.h"
#include "renderer/game/renderer.h"
#include "renderer/game/renderer_helper.h"

namespace simulation {

// updates the game state with a constant rate of @param dt
void update(GameState &gameState, const InputState &input, const double_t dt)
{
  // mouse
  const fixed_t mouseMovement = DoubleToFixed(static_cast<double>(input.mouse_dx) * config::MOUSE_SENSITIVITY);
  const angle_t angleDiff = (mouseMovement < 0 ? 1 : -1) * tantoangle[SlopeDiv(std::abs(mouseMovement), FOCAL_LENGTH)];
  gameState.playerState.angle += angleDiff;

  fixed_t &x = gameState.playerState.x, &y = gameState.playerState.y;

  // Movement should be based on the current facing angle, not per-frame mouse delta.
  const angle_t viewAngle = gameState.playerState.angle;
  const fixed_t sin = finesine[viewAngle >> ANGLE_TO_FINE_SHIFT];
  const fixed_t cos = finesine[(viewAngle + ANG90) >> ANGLE_TO_FINE_SHIFT];

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

  // Forward = (cos, sin), right strafe = (sin, -cos) in this angle system.
  x += FixedMul(FixedMul(moveForward, cos) + FixedMul(moveSide, sin), dis);
  y += FixedMul(FixedMul(moveForward, sin) - FixedMul(moveSide, cos), dis);
}

}// namespace simulation