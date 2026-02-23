#pragma once

#include "sdl_window.h"

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