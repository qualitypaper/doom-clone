#include "framebuffer.h"

namespace framebuffer {
bool isOutOfBounds(uint16_t y) { return (y >= config::WINDOW_HEIGHT); }

void FrameBuffer::drawHorizontalLine(int16_t y, int16_t x0, int16_t x1, uint32_t color)
{
  // scale the point to the window size
  uint16_t scaledY = config::SCALE_Y * y;
  uint16_t transformedX0 = config::SCALE_X * x0;
  uint16_t transformedX1 = config::SCALE_X * x1;

  if (transformedX0 > transformedX1) {
    auto temp = transformedX0;
    transformedX0 = transformedX1;
    transformedX1 = transformedX0;
  }

  // using window width as the pitch, because the current
  // implementation doesn't leave any extra pixels
  uint16_t pitch = config::WINDOW_WIDTH;

  for (uint16_t i = 0; i < config::SCALE_Y + 1; i++) {
    uint32_t *ptr = this->pixels + (scaledY + i) * pitch + (transformedX0);

    for (uint16_t x = transformedX0; x <= transformedX1; x++) {
      *(uint32_t *)ptr = color;
      ptr++;
    }
  }
}

void FrameBuffer::drawVerticalLine(int16_t x, int16_t y0, int16_t y1, uint32_t color)
{
  // scale the point to the window size
  int16_t scaledX = config::SCALE_X * x;
  int16_t transformedY0 = config::SCALE_Y * y0;
  int16_t transformedY1 = config::SCALE_Y * y1;

  if (scaledX < 0 || scaledX > config::WINDOW_WIDTH - 1 || transformedY0 < 0 || transformedY0 > config::WINDOW_HEIGHT - 1
      || transformedY1 < 0 || transformedY1 > config::WINDOW_HEIGHT - 1) {
    return;
  }

  if (transformedY0 > transformedY1) {
    transformedY0 ^= transformedY1;
    transformedY1 ^= transformedY0;
    transformedY0 ^= transformedY1;
  }

  // using window width as the pitch, because the current
  // implementation doesn't leave any extra pixels
  uint32_t pitch = config::WINDOW_WIDTH;

  for (uint32_t i = 0; i < config::SCALE_X + 1; i++) {
    uint32_t *ptr = this->pixels + (transformedY0)*pitch + (scaledX + i);

    for (uint32_t y = transformedY0; y <= transformedY1; y++) {
      *(uint32_t *)ptr = color;
      ptr += pitch;
    }
  }
}

void FrameBuffer::update()
{
  sdl_window::updatePixels(this->pixels);
  sdl_window::updateScreen();
}

void FrameBuffer::pollEvents()
{
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      // destroy
      break;
    case SDL_KEYDOWN:

      break;
    }
  }
}
}// namespace framebuffer