#ifndef DOOMCLONE_LOG_H
#define DOOMCLONE_LOG_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <format>
#include <utility>

class Log
{
public:
  enum class Channel : uint8_t { Core = 0, Client = 1 };
  enum class Level : uint8_t { Trace = 0, Info, Warn, Error, Critical, Off };

  static void Init(const Level level = DefaultLevel())
  {
    s_level.store(level, std::memory_order_relaxed);
    s_initialized.store(true, std::memory_order_relaxed);
  }

  static void SetLevel(const Level level)
  {
    s_level.store(level, std::memory_order_relaxed);
  }

  [[nodiscard]] static Level GetLevel()
  {
    return s_level.load(std::memory_order_relaxed);
  }

  template<typename... Args>
  static void CoreTrace(const std::string_view fmt, Args &&...args)
  {
    Write(Channel::Core, Level::Trace, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  static void CoreInfo(const std::string_view fmt, Args &&...args)
  {
    Write(Channel::Core, Level::Info, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  static void CoreWarn(const std::string_view fmt, Args &&...args)
  {
    Write(Channel::Core, Level::Warn, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  static void CoreError(const std::string_view fmt, Args &&...args)
  {
    Write(Channel::Core, Level::Error, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  static void CoreCritical(const std::string_view fmt, Args &&...args)
  {
    Write(Channel::Core, Level::Critical, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  static void Trace(const std::string_view fmt, Args &&...args)
  {
    Write(Channel::Client, Level::Trace, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  static void Info(const std::string_view fmt, Args &&...args)
  {
    Write(Channel::Client, Level::Info, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  static void Warn(const std::string_view fmt, Args &&...args)
  {
    Write(Channel::Client, Level::Warn, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  static void Error(const std::string_view fmt, Args &&...args)
  {
    Write(Channel::Client, Level::Error, fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  static void Critical(const std::string_view fmt, Args &&...args)
  {
    Write(Channel::Client, Level::Critical, fmt, std::forward<Args>(args)...);
  }

private:
  template<typename... Args>
  static void Write(const Channel channel, const Level level, const std::string_view fmt, Args &&...args)
  {
    EnsureInitialized();

    if (!ShouldLog(level)) {
      return;
    }

    const std::string message = Format(fmt, std::forward<Args>(args)...);

    std::lock_guard lock(s_lock);
    std::ostream &out = (level >= Level::Error) ? std::cerr : std::cout;
    out << '[' << Timestamp() << "] [" << ChannelName(channel) << "] [" << LevelName(level) << "] " << message << '\n';
    out.flush();

    if (level == Level::Critical) {
      std::abort();
    }
  }

  static bool ShouldLog(const Level level)
  {
    const uint8_t current = static_cast<uint8_t>(s_level.load(std::memory_order_relaxed));
    const uint8_t incoming = static_cast<uint8_t>(level);
    return incoming >= current && level != Level::Off;
  }

  static void EnsureInitialized()
  {
    if (!s_initialized.load(std::memory_order_relaxed)) {
      Init();
    }
  }

  static std::string Format(const std::string_view fmt)
  {
    return std::string(fmt);
  }

  template<typename... Args>
  static std::string Format(const std::string_view fmt, Args &&...args)
  {
    try {
      return std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
    } catch (const std::format_error &e) {
      return std::string(fmt) + " [format_error: " + e.what() + ']';
    }
  }

  [[nodiscard]] static std::string Timestamp()
  {
    using Clock = std::chrono::system_clock;
    const auto now = Clock::now();
    const std::time_t timeNow = Clock::to_time_t(now);

    std::tm localTm{};
#if defined(_WIN32)
    localtime_s(&localTm, &timeNow);
#else
    localtime_r(&timeNow, &localTm);
#endif

    std::ostringstream ss;
    ss << std::put_time(&localTm, "%H:%M:%S");
    return ss.str();
  }

  [[nodiscard]] static const char *ChannelName(const Channel channel)
  {
    switch (channel) {
      case Channel::Core:
        return "CORE";
      case Channel::Client:
        return "APP";
      default:
        return "UNKNOWN";
    }
  }

  [[nodiscard]] static const char *LevelName(const Level level)
  {
    switch (level) {
      case Level::Trace:
        return "TRACE";
      case Level::Info:
        return "INFO";
      case Level::Warn:
        return "WARN";
      case Level::Error:
        return "ERROR";
      case Level::Critical:
        return "CRITICAL";
      case Level::Off:
        return "OFF";
      default:
        return "UNKNOWN";
    }
  }

  [[nodiscard]] static constexpr Level DefaultLevel()
  {
#ifdef NDEBUG
    return Level::Info;
#else
    return Level::Trace;
#endif
  }

private:
  inline static std::atomic<bool> s_initialized{ false };
  inline static std::atomic<Level> s_level{ DefaultLevel() };
  inline static std::mutex s_lock;
};

#define DOOM_CORE_TRACE(...) ::Log::CoreTrace(__VA_ARGS__)
#define DOOM_CORE_INFO(...) ::Log::CoreInfo(__VA_ARGS__)
#define DOOM_CORE_WARN(...) ::Log::CoreWarn(__VA_ARGS__)
#define DOOM_CORE_ERROR(...) ::Log::CoreError(__VA_ARGS__)
#define DOOM_CORE_CRITICAL(...) ::Log::CoreCritical(__VA_ARGS__)
#define DOOM_CORE_FATAL(...) ::Log::CoreCritical(__VA_ARGS__)

#define DOOM_TRACE(...) ::Log::Trace(__VA_ARGS__)
#define DOOM_INFO(...) ::Log::Info(__VA_ARGS__)
#define DOOM_WARN(...) ::Log::Warn(__VA_ARGS__)
#define DOOM_ERROR(...) ::Log::Error(__VA_ARGS__)
#define DOOM_CRITICAL(...) ::Log::Critical(__VA_ARGS__)
#define DOOM_FATAL(...) ::Log::Critical(__VA_ARGS__)

#endif// DOOMCLONE_LOG_H
