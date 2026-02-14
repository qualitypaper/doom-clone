#include "framebuffer.h"

namespace framebuffer {
bool isOutOfBounds(uint16_t y) { return (y >= config::WINDOW_HEIGHT); }

FrameBuffer::FrameBuffer(SdlWindow &sdlWindow) : sdlWindow(sdlWindow), width(sdlWindow.width), height(sdlWindow.height)
{
  const uint32_t n = width * height;
  this->pixels = new uint32_t[n];

  reset();
  update();
}

FrameBuffer::~FrameBuffer() {
  delete[] pixels;
}

void FrameBuffer::update() const
{
  sdlWindow.updatePixels(this->pixels);
  sdlWindow.updateScreen();
}

void FrameBuffer::reset() const {
    memset(this->pixels, 0, width * height * sizeof(uint32_t));
}

}// namespace framebuffer
