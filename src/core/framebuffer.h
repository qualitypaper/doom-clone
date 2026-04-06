#pragma once

#include "sdl_window.h"

#include <memory>

class FrameBuffer
{
private:
  std::shared_ptr<SdlWindow> sdlWindow;

public:
  uint16_t width;
  uint16_t height;

  uint32_t *pixels;

  explicit FrameBuffer(std::shared_ptr<SdlWindow> sdlWindow);
  ~FrameBuffer();

  void update() const;
  void reset() const;
};