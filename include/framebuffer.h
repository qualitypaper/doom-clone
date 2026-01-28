#pragma once

#include <iostream>

#include <glm/glm.hpp>

#include "config.h"
#include "sdl_window.h"

namespace framebuffer {

class FrameBuffer
{
private:
  int16_t width;
  int16_t height;

public:
  uint32_t *pixels;
  FrameBuffer(uint16_t width, uint16_t height);
  ~FrameBuffer();

  int16_t getWidth() { return width; }
  int16_t getHeight() { return height; }

  void update();
  void reset();
};
}// namespace framebuffer