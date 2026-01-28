#include "framebuffer.h"

namespace framebuffer {
bool isOutOfBounds(uint16_t y) { return (y >= config::WINDOW_HEIGHT); }

FrameBuffer::FrameBuffer(uint16_t width, uint16_t height) : width(width), height(height)
{

  uint32_t n = width * height;
  this->pixels = new uint32_t[n];

  for (uint32_t i = 0; i < n; i++) { this->pixels[i] = 0xFFFFFFFF; }

  // SDL should already be initialized by now
  sdl_window::updatePixels(this->pixels);
  sdl_window::updateScreen();
}

FrameBuffer::~FrameBuffer() {
  delete[] pixels;
}

void FrameBuffer::update()
{
  sdl_window::updatePixels(this->pixels);
  sdl_window::updateScreen();
}

void FrameBuffer::reset()
{
  for (int i = 0; i < sizeof(this->pixels); i++) { this->pixels[i] = 0; }
}

}// namespace framebuffer
