//
// Created by qualitypaper on 3/30/26.
//

#ifndef DOOMCLONE_TABLES_H
#define DOOMCLONE_TABLES_H

#include <array>
#include <cstdint>

typedef uint32_t angle_t;

constexpr int16_t FINE_ANGLES = 8192;
constexpr angle_t ANGLE_TO_FINE_SHIFT = 19;

// effective size is 10240
extern std::array<int16_t, 5 * FINE_ANGLES / 4> finesine{};
// re-use data, is just pi/2 phase shift
extern const int16_t* finecosine;

// effective size is 4096
extern std::array<int16_t, FINE_ANGLES/2> finetangent{};

// BAM (Binary Angle Measurement)
constexpr angle_t ANG45 = 0x20000000;
constexpr angle_t ANG90 = 0x40000000;
constexpr angle_t ANG180 = 0x80000000;
constexpr angle_t ANG270 = 0xC0000000;

constexpr angle_t HALF_FOV = ANG45;
constexpr angle_t FOV = ANG90;

constexpr int16_t SLOPE_RANGE = 2048;

extern std::array<angle_t, SLOPE_RANGE+1> tantoangle{};

#endif// DOOMCLONE_TABLES_H
