#include "framebuffer.h"

namespace framebuffer {
bool isOutOfBounds(uint16_t y) { return (y >= config::WINDOW_HEIGHT); }

FrameBuffer(SdlWindow &sdlWindow) : sdlWindow(sdlWindow), width(sdlWindow.width), height(sdlWindow.height)
{
  const uint32_t n = width * height;
  this->pixels = new uint32_t[n];

  reset();
  update();
}

~FrameBuffer() {
  delete[] pixels;
}

void update() const
{
  sdlWindow.updatePixels(this->pixels);
  sdlWindow.updateScreen();
}

void reset() const {
    memset(this->pixels, 0, width * height * sizeof(uint32_t));
}

}// namespace framebuffer
