#pragma once
#include <fmt/core.h>
#include <format>


template<typename... Args> class DelayedLogger
{
public:
  explicit DelayedLogger(const fmt::string_view _format, const size_t _delay = 0) : format(_format), delay(_delay) {}

  void log(const Args &&...args)
  {
    if (delayCounter++ >= delay) {
      fmt::println("{}", fmt::vformat(format, fmt::make_format_args(args...)));
      delayCounter = 0;
    }
  }

private:
  fmt::string_view format;
  size_t delayCounter = 0, delay;
};