#include "framebuffer.h"
#include "config.h"

FrameBuffer::FrameBuffer(SdlWindow &sdlWindow)
  : sdlWindow(sdlWindow), width(sdlWindow.getRenderWidth()), height(sdlWindow.getRenderHeight())
{
  const uint32_t n = width * height;
  this->pixels = new uint32_t[n];

  reset();
  update();
}

FrameBuffer::~FrameBuffer() { delete[] pixels; }

void FrameBuffer::update() const
{
  sdlWindow.updatePixels(this->pixels);
  sdlWindow.updateScreen();
}

void FrameBuffer::reset() const { memset(this->pixels, UINT32_MAX, width * height * sizeof(uint32_t)); }
