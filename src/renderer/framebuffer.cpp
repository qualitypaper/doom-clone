#include "framebuffer.h"

namespace framebuffer {
bool isOutOfBounds(uint16_t y) { return (y >= config::WINDOW_HEIGHT); }

FrameBuffer::FrameBuffer(sdl_window::SdlWindow &sdlWindow) : sdlWindow(sdlWindow), width(sdlWindow.width), height(sdlWindow.height)
{
  uint32_t n = width * height;
  this->pixels = new uint32_t[n];

  reset();
  update();
}

FrameBuffer::~FrameBuffer() {
  delete[] pixels;
}

void FrameBuffer::update()
{
  sdlWindow.updatePixels(this->pixels);
  sdlWindow.updateScreen();
}

void FrameBuffer::reset()
{
  for (int i = 0; i < sizeof(this->pixels); i++) { this->pixels[i] = 0; }
}

}// namespace framebuffer
