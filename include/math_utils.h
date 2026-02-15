#pragma once

#include "editor.h"
#include "gameloop.h"
#include "imgui.h"

namespace math_utils {

constexpr double PI = 3.141592653589793;

constexpr double toRadians(const double angle) { return PI * angle / 180; }

constexpr Vertex toCenterCoordinates(Vertex vec, uint16_t width, uint16_t height)
{ return Vertex(vec.x - width / 2, height / 2 - vec.y); }
constexpr Vertex toCenterCoordinates(const EditorVertex &vec, uint16_t width, uint16_t height)
{ return Vertex(vec.x - width / 2, height / 2 - vec.y); }

constexpr ImVec2 fromCenterCoordinates(ImVec2 vec, uint16_t width, uint16_t height)
{
  return ImVec2(std::max(0.0f, std::min(static_cast<float>(width), width / 2 + vec.x)),
    std::max(0.0f, std::min(static_cast<float>(height), height / 2 - vec.y)));
}

constexpr ImVec2 convertVertexIntoImVec2(Vertex vertex) { return ImVec2(vertex.y, vertex.x); }

constexpr Vertex convertImVec2IntoVertex(ImVec2 v) { return { static_cast<int16_t>(v.x), static_cast<int16_t>(v.y) }; };

constexpr float getDistanceSq(float x1, float y1, float x2, float y2)
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

// returns the parameter for the second line
// so the solution will be: p2 + returnValue * d2
constexpr double findLinesIntersection(const Vertex p1, const Vertex d1, const Vertex p2, const Vertex d2)
{
  if (d1.x != 0) {
    const double temp = static_cast<double>(d1.y) / static_cast<double>(d1.x);
    return (p2.y - p1.y - temp * (p2.x - p1.x)) / (temp * d2.x - d2.y);
  } else if (d1.y != 0) {
    return -(p2.x - p1.x) / static_cast<double>(p2.x);
  } else {
    throw std::runtime_error("Direction is a null vector.");
  }
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
