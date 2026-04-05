#include "core/framebuffer.h"
#include "renderer.h"

namespace {


fixed_t s_baseXScale;
fixed_t s_baseYScale;
}// namespace


void Renderer::MakeSpans(const Visplane &plane, const int x, int t1, int b1, int t2, int b2) const
{
  static std::array<int, CANVAS_HEIGHT> s_spanStart;

  while (t1 > 0 && t1 < t2 && t1 <= b1) {
    MapPlane(plane, t1, s_spanStart[t1], x - 1);
    t1++;
  }

  while (b1 < CANVAS_HEIGHT && b1 > b2 && b1 >= t1) {
    MapPlane(plane, b1, s_spanStart[b1], x - 1);
    b1--;
  }

  while (t2 > 0 && t2 < t1 && t2 <= b2) {
    s_spanStart[t2] = x;
    t2++;
  }

  while (b2 < CANVAS_HEIGHT && b2 > b1 && b2 >= t2) {
    s_spanStart[b2] = x;
    b2--;
  }
}

void Renderer::DrawSpan(const drawseg_t &ds) const
{
  // scale the point to the window size
  int scaledY = static_cast<int>(SCALE_X * ds.y);
  int transformedX1 = static_cast<int>(SCALE_Y * ds.x1);
  int transformedX2 = static_cast<int>(SCALE_Y * ds.x2);

  scaledY = std::clamp(scaledY, 0, WINDOW_HEIGHT - 1);
  transformedX1 = std::clamp(transformedX1, 0, WINDOW_WIDTH - 1);
  transformedX2 = std::clamp(transformedX2, 0, WINDOW_WIDTH - 1);

  if (transformedX1 > transformedX2)
    std::swap(transformedX1, transformedX2);

  // using window width as the pitch, because the current
  // implementation doesn't leave any extra pixels
  const uint32_t pitch = this->m_fb.width;

  for (uint32_t i = 0; i <= SCALE_X + 1; i++) {
    uint32_t *ptr = this->m_fb.pixels + scaledY * pitch + (transformedX1 + i);

    for (uint32_t *curr = ptr; curr <= ptr + transformedX2; curr++) {
      *(uint32_t *)curr = ds.color;
    }
  }
}

void Renderer::MapPlane(const Visplane &plane, const int y, const int x1, const int x2) const
{
  const fixed_t distance = FixedMul(plane.height - m_player->z, m_ySlope[y]);
  const fixed_t xStep = FixedMul(s_baseXScale, distance);
  const fixed_t yStep = FixedMul(s_baseYScale, distance);

  const fixed_t length = FixedMul(distance, m_distScale[x1]);
  const angle_t angle = m_player->angle + m_xToViewAngle[x1];

  const fixed_t xFrac = CANVAS_CENTERX_FRAC + FixedMul(finesine[(angle + ANG90) >> ANGLE_TO_FINE_SHIFT], length);
  const fixed_t yFrac = -CANVAS_CENTERY_FRAC - FixedMul(finesine[angle >> ANGLE_TO_FINE_SHIFT], length);

  const drawseg_t ds{ y, x1, x2, xStep, yStep, xFrac, yFrac, plane.color };
  DrawSpan(ds);
}

void Renderer::ResetPlanes()
{
  std::ranges::fill(m_ceilClip, 0);
  std::ranges::fill(m_floorClip, m_canvasHeight - 1);
  m_visplanes.clear();

  // left to right mapping
  const angle_t angle = (m_player->angle - ANG90) >> ANGLE_TO_FINE_SHIFT;

  // use cos for x
  s_baseXScale = FixedDiv(finesine[m_player->angle >> ANGLE_TO_FINE_SHIFT], FOCAL_LENGTH);
  s_baseYScale = -FixedDiv(finesine[angle], FOCAL_LENGTH);
}

// TODO: change color to texture implementation
Visplane *Renderer::FindVisPlane(const fixed_t height, const uint32_t color, const int16_t lightLevel)
{
  for (Visplane &vp : m_visplanes) {
    if (vp.height == height && vp.color == color && vp.lightLevel == lightLevel) {
      return &vp;
    }
  }

  m_visplanes.emplace_back(height, color, lightLevel, CANVAS_WIDTH, -1);
  Visplane *newPlane = &m_visplanes.back();

  memset(newPlane->top, -1, sizeof(newPlane->top));

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
  Visplane &lastVisplane = m_visplanes.back();
  lastVisplane.height = visplane->height;
  lastVisplane.lightLevel = visplane->lightLevel;
  lastVisplane.color = visplane->color;


  visplane = &m_visplanes.emplace_back();
  visplane->minX = start;
  visplane->maxX = end;

  memset(visplane->top, -1, sizeof(visplane->top));

  return visplane;
}

void Renderer::RenderVisPlanes()
{
  for (Visplane &vp : m_visplanes) {
    if (vp.minX > vp.maxX)
      continue;

    vp.top[vp.minX - 1] = -1;
    vp.top[vp.maxX + 1] = -1;

    for (int x = vp.minX; x <= vp.maxX; x++) {
      MakeSpans(vp, x, vp.top[x - 1], vp.bottom[x - 1], vp.top[x], vp.bottom[x]);
    }
  }
}
