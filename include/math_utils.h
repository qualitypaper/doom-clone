#pragma once

#include "imgui.h"

namespace math_utils {
constexpr float getDistanceSq(ImVec2 a, ImVec2 b)
{

  float x = a.x - b.x;
  float y = a.y - b.y;

  return x * x + y * y;
}
constexpr float dotProduct(ImVec2 a, ImVec2 b) { return a.x * b.x + a.y * b.y; }
}// namespace math_utils