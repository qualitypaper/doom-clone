//
// Created by qualitypaper on 3/31/26.
//

#ifndef DOOMCLONE_FIXED_MATH_H
#define DOOMCLONE_FIXED_MATH_H
typedef int fixed_t;

constexpr int FRAC_BITS = 16;
constexpr int FRAC_UNIT = 1 << FRAC_BITS;

constexpr fixed_t FixedMul(fixed_t a, fixed_t b);
constexpr fixed_t FixedDiv(fixed_t a, fixed_t b);
constexpr fixed_t FixedDiv2(fixed_t a, fixed_t b);

constexpr fixed_t DoubleToFixed(double num);
constexpr double FixedToDouble(fixed_t num);


#endif// DOOMCLONE_FIXED_MATH_H
