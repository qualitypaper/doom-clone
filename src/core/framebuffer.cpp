#include "framebuffer.h"
#include "config.h"

FrameBuffer::FrameBuffer(std::shared_ptr<SdlWindow> sdlWindow)
  : sdlWindow(sdlWindow), width(sdlWindow->GetRenderWidth()), height(sdlWindow->GetRenderHeight())
{
  const uint32_t n = width * height;
  this->pixels = new uint32_t[n];

  reset();
  update();
}

FrameBuffer::~FrameBuffer() { delete[] pixels; }

void FrameBuffer::update() const
{
  sdlWindow->UpdatePixels(this->pixels);
  sdlWindow->UpdateScreen();
}

void FrameBuffer::reset() const { memset(this->pixels, UINT32_MAX, width * height * sizeof(uint32_t)); }
