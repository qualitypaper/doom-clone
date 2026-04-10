#include "core/framebuffer.h"
#include "core/gameloop.h"
#include "renderer.h"

#include <algorithm>

namespace {
fixed_t s_baseXScale;
fixed_t s_baseYScale;
}// namespace


void Renderer::MakeSpans(const Visplane &plane, const int x, int t1, int b1, int t2, int b2)
{
  const int maxY = static_cast<int>(m_fb.height);

  while (t1 >= 0 && t1 < maxY && t1 < t2 && t1 <= b1) {
    MapPlane(plane, t1, m_spanStart[t1], x - 1);
    t1++;
  }

  while (b1 < maxY && b1 > b2 && b1 >= t1) {
    MapPlane(plane, b1, m_spanStart[b1], x - 1);
    b1--;
  }

  while (t2 >= 0 && t2 < maxY && t2 < t1 && t2 <= b2) {
    m_spanStart[t2] = x;
    t2++;
  }

  while (b2 < maxY && b2 >= 0 && b2 > b1 && b2 >= t2) {
    m_spanStart[b2] = x;
    b2--;
  }
}

void Renderer::DrawSpan(drawspan_t &ds) const
{
  // scale the point to the window size
  ds.y = std::clamp(ds.y, 0, m_fb.height - 1);
  ds.x1 = std::clamp(ds.x1, 0, m_fb.width - 1);
  ds.x2 = std::clamp(ds.x2, 0, m_fb.width - 1);

  if (ds.x1 > ds.x2)
    std::swap(ds.x1, ds.x2);

  // using window width as the pitch, because the current
  // implementation doesn't leave any extra pixels
  const uint32_t pitch = this->m_fb.width;

  uint32_t *ptr = this->m_fb.pixels + ds.y * pitch + ds.x1;

  for (uint32_t *curr = ptr; curr <= ptr + (ds.x2 - ds.x1); curr++) {
    *(uint32_t *)curr = ds.color;
  }
}

void Renderer::MapPlane(const Visplane &plane, const int y, const int x1, const int x2) const
{
  const fixed_t distance = FixedMul(std::abs(plane.height - m_player->z), m_ySlope[y]);
  const fixed_t xStep = FixedMul(s_baseXScale, distance);
  const fixed_t yStep = FixedMul(s_baseYScale, distance);

  const fixed_t length = FixedMul(distance, m_distScale[x1]);
  const angle_t angle = m_player->angle + m_xToViewAngle[x1];

  const fixed_t xFrac = m_centerXFrac + FixedMul(finesine[(angle + ANG90) >> ANGLE_TO_FINE_SHIFT], length);
  const fixed_t yFrac = -m_centerYFrac - FixedMul(finesine[angle >> ANGLE_TO_FINE_SHIFT], length);

  drawspan_t ds{ y, x1, x2, xStep, yStep, xFrac, yFrac, plane.color };
  DrawSpan(ds);
}

void Renderer::ResetPlanes()
{
  std::ranges::fill(m_ceilClip, 0);
  std::ranges::fill(m_floorClip, m_fb.height - 1);
  m_visplanes.clear();

  // left to right mapping
  const angle_t angle = (m_player->angle - ANG90) >> ANGLE_TO_FINE_SHIFT;

  // use cos for x
  s_baseXScale = FixedDiv(finesine[m_player->angle >> ANGLE_TO_FINE_SHIFT], m_focalLength);
  s_baseYScale = -FixedDiv(finesine[angle], m_focalLength);
}

// TODO: change color to texture implementation
Visplane *Renderer::FindVisPlane(const fixed_t height, const uint32_t color, const int16_t lightLevel)
{
  for (Visplane &vp : m_visplanes) {
    if (vp.height == height && vp.color == color && vp.lightLevel == lightLevel) {
      return &vp;
    }
  }

  m_visplanes.emplace_back(height, color, lightLevel, static_cast<int>(m_fb.width), -1, m_fb.width);
  Visplane *newPlane = &m_visplanes.back();

  std::ranges::fill(newPlane->top, -1);
  std::ranges::fill(newPlane->bottom, -1);

  return newPlane;
}


Visplane *Renderer::CheckVisPlane(Visplane *visplane, const int start, const int end)
{
  if (!visplane)
    return nullptr;

  if (end <= start)
    return visplane;

  int internalLow, internalHigh;
  int unionLow, unionHigh;

  if (start < visplane->minX) {
    internalLow = visplane->minX;
    unionLow = start;
  } else {
    internalLow = start;
    unionLow = visplane->minX;
  }

  if (end > visplane->maxX) {
    internalHigh = visplane->maxX;
    unionHigh = end;
  } else {
    unionHigh = visplane->maxX;
    internalHigh = end;
  }

  int x;

  // look for changed y's
  for (x = internalLow; x <= internalHigh; x++) {
    if (visplane->top[x] != -1) {
      break;
    }
  }

  if (x > internalHigh) {
    // use the same one
    visplane->minX = unionLow;
    visplane->maxX = unionHigh;

    return visplane;
  }

  // make a new visplane
  const fixed_t sourceHeight = visplane->height;
  const int sourceLightLevel = visplane->lightLevel;
  const uint32_t sourceColor = visplane->color;

  visplane = &m_visplanes.emplace_back(sourceHeight, sourceColor, sourceLightLevel, start, end, m_fb.width);

  return visplane;
}

void Renderer::RenderVisPlanes()
{
  for (Visplane &vp : m_visplanes) {
    if (vp.minX > vp.maxX)
      continue;

    int prevTop = -1;
    int prevBottom = -1;

    // Iterate one column past maxX to flush pending spans.
    for (int x = vp.minX; x <= vp.maxX + 1; x++) {
      int curTop = -1;
      int curBottom = -1;

      if (x <= vp.maxX) {
        curTop = vp.top[x];
        curBottom = vp.bottom[x];
      }

      MakeSpans(vp, x, prevTop, prevBottom, curTop, curBottom);
      prevTop = curTop;
      prevBottom = curBottom;
    }
  }
}
