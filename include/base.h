#ifndef DOOMCLONE_BASE_H
#define DOOMCLONE_BASE_H

#ifdef DOOM_DEBUG
#if defined(_WIN32) || defined(_WIN64)
#define DOOM_DEBUGBREAK() __debugbreak()
#elif defined(__linux__)
#define DOOM_DEBUGBREAK() __builtin_trap()
#else
#error "Platform doesn't support debug break yet!"
#endif

#define DOOM_ENABLE_ASSERTS
#else
#define DOOM_DEBUGBREAK()
#endif

#define DOOM_STRINGIFY(x) #x
#define DOOM_EXPAND(x) x

#endif// DOOMCLONE_BASE_H
