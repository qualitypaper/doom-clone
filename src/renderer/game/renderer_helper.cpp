#include "renderer_helper.h"

#include "core/gameloop.h"
#include "tables.h"

uint32_t SlopeDiv(const uint32_t num, const uint32_t den)
{
  // denominator is too small
  if (den < 512) {
    return SLOPE_RANGE;
  }

  // transforming num/den into tantoangle range
  // 1.0 is the max value, 0.0 the minimal, the biggest possible index SLOPE_RANGE
  const uint32_t ans = (num << 3) / (den >> 8);

  return ans <= SLOPE_RANGE ? ans : SLOPE_RANGE;
}

// faster alternative for atan function
angle_t PointToAngle(fixed_t x, fixed_t y, const fixed_t viewx, const fixed_t viewy)
{
  x -= viewx;
  y -= viewy;

  if (!x && !y) {
    return 0;
  }

  // determining the angle depending on the octant the point is located
  if (x >= 0) {
    if (y >= 0) {
      if (x > y) {
        return tantoangle[SlopeDiv(y, x)];
      } else {
        return ANG90 - 1 - tantoangle[SlopeDiv(x, y)];
      }
    } else {
      y = -y;
      if (x > y) {
        return -tantoangle[SlopeDiv(y, x)];
      } else {
        return ANG270 + tantoangle[SlopeDiv(x, y)];
      }
    }
  } else {
    x = -x;
    if (y >= 0) {
      if (x > y) {
        return ANG180 - 1 - tantoangle[SlopeDiv(y, x)];
      } else {
        return ANG90 + tantoangle[SlopeDiv(x, y)];
      }
    } else {
      y = -y;
      if (x > y) {
        return ANG180 + tantoangle[SlopeDiv(y, x)];
      } else {
        return ANG270 - 1 - tantoangle[SlopeDiv(x, y)];
      }
    }
  }

  return 0;
}

angle_t PointToAngle2(const fixed_t x1, const fixed_t y1, const fixed_t x2, const fixed_t y2)
{
  return PointToAngle(x2, y2, x1, y1);
}

fixed_t PointToDist(const fixed_t x, const fixed_t y, const Player &player)
{
  fixed_t dx = std::abs(player.x - x);
  fixed_t dy = std::abs(player.y - y);

  if (dy > dx) {
    std::swap(dx, dy);
  }

  if (dx == 0)
    return 0;

  const angle_t angle = (tantoangle[SlopeDiv(dy, dx)] + ANG90) >> ANGLE_TO_FINE_SHIFT;

  // uses cosine
  return FixedDiv(dx, finesine[angle]);
}
