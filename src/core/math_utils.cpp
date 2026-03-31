#include "math_utils.h"
#include <utility>

namespace math_utils {
std::pair<fixed_t, fixed_t> FindLinesIntersection(const Vertex p1, const Vertex d1, const Vertex p2, const Vertex d2)
{
  const fixed_t det = FixedMul(d1.x, d2.y) - FixedMul(d1.y, d2.x);

  if (det >> SLOPE_BITS == 0) {
    return std::pair(0, 0);
  }

  const Vertex b = p2 - p1;

  // multiplication of the inverse matrix with b
  const fixed_t firstSol = FixedMul(FixedDiv(1, det), FixedMul(d2.y, b.x) - FixedMul(d2.x, b.y));
  const fixed_t secondSol = FixedMul(FixedDiv(1, det), -FixedMul(d1.y, b.x) + FixedMul(d1.x, b.y));

  return std::pair(firstSol, secondSol);
}

}// namespace math_utils
