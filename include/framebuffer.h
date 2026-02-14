#pragma once

#include <iostream>

#include <glm/glm.hpp>

#include "config.h"
#include "sdl_window.h"

namespace framebuffer {

class FrameBuffer
{
private:
  SdlWindow &sdlWindow;

public:
  uint16_t width;
  uint16_t height;

  uint32_t *pixels;

  explicit FrameBuffer(SdlWindow &sdlWindow);
  ~FrameBuffer();

  void update() const;
  void reset() const;
};
}// namespace framebuffer