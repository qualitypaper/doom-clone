#pragma once

#include "editor.h"
#include "gameloop.h"
#include "imgui.h"

namespace math_utils {

constexpr gameloop::Vertex toCenterCoordinates(gameloop::Vertex vec, uint16_t width, uint16_t height) {
  return gameloop::Vertex(vec.x - width /2, height/2 - vec.y);
}

constexpr ImVec2 fromCenterCoordinates(ImVec2 vec, uint16_t width, uint16_t height)
{
  return ImVec2(std::max(0.0f, std::min(static_cast<float>(width), width / 2 + vec.x)),
    std::max(0.0f, std::min(static_cast<float>(height), height / 2 - vec.y)));
}

constexpr ImVec2 convertVertexIntoImVec2(gameloop::Vertex vertex) { return ImVec2(vertex.y, vertex.x); }

constexpr gameloop::Vertex convertImVec2IntoVertex(ImVec2 v)
{
  return { static_cast<int16_t>(v.x), static_cast<int16_t>(v.y) };
};

constexpr float getDistanceSq(float x1, float y1, float x2, float y2)
{

  float x = x2 - x1;
  float y = y1 - y2;

  return x * x + y * y;
}
constexpr float getDistanceSq(ImVec2 a, ImVec2 b) { return getDistanceSq(a.x, a.y, b.x, b.y); }
constexpr float getDistanceSq(gameloop::Vertex v1, gameloop::Vertex v2)
{
  return getDistanceSq(v1.x, v1.y, v2.x, v2.y);
}
constexpr float dotProduct(ImVec2 a, ImVec2 b) { return a.x * b.x + a.y * b.y; }
}// namespace math_utils
