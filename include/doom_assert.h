#ifndef DOOMCLONE_ASSERT_H
#define DOOMCLONE_ASSERT_H

#include "base.h"

#ifdef DOOM_ENABLE_ASSERTS
// Alteratively the same "default" message can be used for both "WITH_MSG" and "NO_MSG" and
// provide support for custom formatting by concatenating the formatting string instead of having the format inside the default message
#define DOOM_INTERNAL_ASSERT_IMPL(type, check, msg, ...) \
  {                                                      \
    if (!(check)) {                                      \
      DOOM##type##ERROR(msg, __VA_ARGS__);                 \
      DOOM_DEBUGBREAK();                                   \
    }                                                    \
  }
#define DOOM_INTERNAL_ASSERT_WITH_MSG(type, check, ...) \
  DOOM_INTERNAL_ASSERT_IMPL(type, check, "Assertion failed: {0}", __VA_ARGS__)
#define DOOM_INTERNAL_ASSERT_NO_MSG(type, check)                                 \
  DOOM_INTERNAL_ASSERT_IMPL(type,                                                \
                            check,                                               \
                            "Assertion '{0}' failed at {1}:{2}",                 \
                            DOOM_STRINGIFY(check),                           \
                            std::filesystem::path(__FILE__).filename().string(), \
                            __LINE__)

#define DOOM_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
#define DOOM_INTERNAL_ASSERT_GET_MACRO(...) \
  DOOM_EXPAND(                              \
    DOOM_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, DOOM_INTERNAL_ASSERT_WITH_MSG, DOOM_INTERNAL_ASSERT_NO_MSG))

// Currently accepts at least the condition and one additional parameter (the message) being optional
#define DOOM_ASSERT(...) DOOM_EXPAND(DOOM_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_, __VA_ARGS__))
#define DOOM_CORE_ASSERT(...) DOOM_EXPAND(DOOM_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__))
#else
#define DOOM_ASSERT(...)
#define DOOM_CORE_ASSERT(...)
#endif

#endif// DOOMCLONE_ASSERT_H
