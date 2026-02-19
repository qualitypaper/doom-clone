#pragma once

#include "editor.h"
#include "gameloop.h"
#include "imgui.h"

namespace math_utils {

constexpr double PI = 3.141592653589793;

constexpr double toRadians(const double angle) { return PI * angle / 180; }

constexpr Vertex toCenterCoordinates(const Vertex vec, const uint16_t width, const uint16_t height)
{ return Vertex(vec.x - width / 2, height / 2 - vec.y); }
constexpr Vertex toCenterCoordinates(const EditorVertex &vec, const uint16_t width, const uint16_t height)
{ return Vertex(vec.x - width / 2, height / 2 - vec.y); }

constexpr ImVec2 fromCenterCoordinates(const ImVec2 vec, const uint16_t width, const uint16_t height)
{
  return { std::max(0.0f, std::min(static_cast<float>(width), static_cast<float>(width) / 2 + vec.x)),
    std::max(0.0f, std::min(static_cast<float>(height), static_cast<float>(height) / 2 - vec.y)) };
}

constexpr Vertex fromCenterCoordinates(const Vertex vec, const uint16_t width, const uint16_t height)
{
  return { std::max(0, std::min(static_cast<int>(width), width / 2 + vec.x)),
    std::max(0, std::min(static_cast<int>(height), height / 2 - vec.y)) };
}

constexpr ImVec2 convertVertexIntoImVec2(const Vertex vertex)
{ return { static_cast<float>(vertex.x), static_cast<float>(vertex.y) }; }

constexpr Vertex convertImVec2IntoVertex(const ImVec2 v)
{ return { static_cast<int16_t>(v.x), static_cast<int16_t>(v.y) }; };

constexpr float getDistanceSq(const float x1, const float y1, const float x2, const float y2)
{

  const float x = x2 - x1;
  const float y = y1 - y2;

  return x * x + y * y;
}

// takes two vectors and returns the cross product (z component is always 0, since we are in 2D)
constexpr int32_t crossProductLength(const Vertex v1, const Vertex v2) { return v1.x * v2.y - v1.y * v2.x; }

constexpr float getDistanceSq(ImVec2 a, ImVec2 b) { return getDistanceSq(a.x, a.y, b.x, b.y); }
constexpr float getDistanceSq(Vertex v1, Vertex v2) { return getDistanceSq(v1.x, v1.y, v2.x, v2.y); }
constexpr float dotProduct(ImVec2 a, ImVec2 b) { return a.x * b.x + a.y * b.y; }


/**
 *
 * @param p1 starting point of the first line
 * @param d1 direction vector of the first line
 * @param p2 starting point of the second line
 * @param d2 direction vector of the second line
 * @return null vector when d1 and d2 are collinear, otherwise a solution to LSE
 */
inline std::pair<double, double>
  findLinesIntersection(const Vertex p1, const Vertex d1, const Vertex p2, const Vertex d2)
{
  const glm::mat2x2 A{ d1.x, d1.y, -d2.x, -d2.y };

  if (glm::determinant(A) == 0) { return std::pair(0, 0); }

  const glm::vec2 b{ p2.x - p1.x, p2.y - p1.y };

  glm::vec2 sol = glm::inverse(A) * b;

  return std::pair(sol.x, sol.y);
}


constexpr Vertex rotateAroundX(const Vertex v, const double angleDegrees)
{
  const double cos = std::cos(toRadians(angleDegrees));
  const double sin = std::sin(toRadians(angleDegrees));

  return { static_cast<int32_t>(cos * v.x - sin * v.y), static_cast<int32_t>(sin * v.x + cos * v.y) };
}

inline ImVec2 rotateAroundX(const EditorVertex &v, const double angleDegrees)
{
  const double cos = std::cos(toRadians(angleDegrees));
  const double sin = std::sin(toRadians(angleDegrees));

  return { static_cast<float>(cos * v.x) - static_cast<float>(sin * v.y),
    static_cast<float>(sin * v.x) + static_cast<float>(cos * v.y) };
}
}// namespace math_utils
