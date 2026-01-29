#pragma once

#include <iostream>

#include <glm/glm.hpp>

#include "config.h"
#include "sdl_window.h"

namespace framebuffer {

class FrameBuffer
{
private:

  sdl_window::SdlWindow &sdlWindow;

public:
  int16_t width;
  int16_t height;

  uint32_t *pixels;

  FrameBuffer(sdl_window::SdlWindow &sdlWindow);
  ~FrameBuffer();

  void update();
  void reset();
};
}// namespace framebuffer