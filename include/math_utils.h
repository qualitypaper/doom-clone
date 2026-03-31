#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#define _USE_MATH_DEFINES

#include "core/fixed_math.h"
#include "defs.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <glm/glm.hpp>

template<typename T>
concept HasXY = requires(T t) {
  { t.x } -> std::convertible_to<float>;
  { t.y } -> std::convertible_to<float>;
};

namespace math_utils {

inline double toRadians(const double angle) { return M_PI * angle / 180; }

template<HasXY T> constexpr T toCenterCoordinates(const T &vec, const uint16_t width, const uint16_t height)
{
  return { vec.x - width / 2, height / 2 - vec.y };
}


template<HasXY T> constexpr T fromCenterCoordinates(const T &vec, const uint16_t width, const uint16_t height)
{
  return {
    decltype(vec.x)(std::clamp(static_cast<double>(width) / 2 + vec.x, 0.0, static_cast<double>(width))),
    decltype(vec.y)(std::clamp(static_cast<double>(height) / 2 - vec.y, 0.0, static_cast<double>(height)))
  };
}


// takes two vectors and returns the cross product (z component is always 0, since we are in 2D)
// shouldn't be used for Vertex objects
template<HasXY T> constexpr int32_t crossProductLengthNDir(const T v1, const T v2) { return v1.x * v2.y - v1.y * v2.x; }

template<typename T> constexpr float getDistanceSq(T v1, T v2)
{
  const float x = v2.x - v1.x;
  const float y = v1.y - v2.y;

  return x * x + y * y;
}

template<HasXY T> constexpr float dotProduct(T a, T b) { return a.x * b.x + a.y * b.y; }

/**
 *
 * @param p1 starting point of the first line
 * @param d1 direction vector of the first line
 * @param p2 starting point of the second line
 * @param d2 direction vector of the second line
 * @return null vector when d1 and d2 are collinear, otherwise a solution to LSE
 */

std::pair<fixed_t, fixed_t> FindLinesIntersection(Vertex p1, Vertex d1, Vertex p2, Vertex d2);

template<HasXY T> T rotateAroundX(const T v, const double angleDegrees)
{
  const double cos = std::cos(toRadians(angleDegrees));
  const double sin = std::sin(toRadians(angleDegrees));

  return { decltype(v.x)(cos * v.x - sin * v.y), decltype(v.y)(sin * v.x + cos * v.y) };
}

}// namespace math_utils
#endif