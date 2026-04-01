#pragma once
#include <format>

template<typename... Args>
class DelayedLogger
{
public:
  explicit DelayedLogger(const std::string_view _format, const size_t _delay = 0) : format(_format), delay(_delay) {}

  void log(const Args &...args)
  {
    if (delayCounter++ >= delay) {
      std::printf(format.data(), args...);
      delayCounter = 0;
    }
  }

private:
  std::string_view format;
  size_t delayCounter = 0, delay;
};