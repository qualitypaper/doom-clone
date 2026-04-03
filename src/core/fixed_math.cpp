#include "fixed_math.h"

#include <limits>
#include <climits>
#include <cmath>
#include <cstdint>

fixed_t FixedMul(const fixed_t a, const fixed_t b)
{
  return (static_cast<long long>(a) * static_cast<long long>(b)) >> FRAC_BITS;
}


fixed_t FixedDiv(const fixed_t a, const fixed_t b)
{
  if (std::abs(a) >> 14 >= std::abs(b)) {
    return (a ^ b) < 0 ? INT_MIN : INT_MAX;
  }

  return FixedDiv2(a, b);
}


fixed_t FixedDiv2(const fixed_t a, const fixed_t b)
{

  const double c = static_cast<double>(a) / static_cast<double>(b) * FRAC_UNIT;

  if (c >= 2147483648.0 || c < -2147483648.0)
    return 0;

  return static_cast<fixed_t>(c);
}

fixed_t DoubleToFixed(double num)
{
  // Handle invalid floating values explicitly.
  if (std::isnan(num)) {
    return 0;
  }
  if (std::isinf(num)) {
    return num > 0.0 ? std::numeric_limits<fixed_t>::max()
                     : std::numeric_limits<fixed_t>::min();
  }

  // Scale into Q16.16 space.
  const double scaled = num * static_cast<double>(FRAC_UNIT);

  // Saturate before integer conversion.
  constexpr double kMax = static_cast<double>(std::numeric_limits<fixed_t>::max());
  constexpr double kMin = static_cast<double>(std::numeric_limits<fixed_t>::min());

  if (scaled >= kMax) {
    return std::numeric_limits<fixed_t>::max();
  }
  if (scaled <= kMin) {
    return std::numeric_limits<fixed_t>::min();
  }

  // Round to nearest integer in fixed-point units.
  return static_cast<fixed_t>(std::llround(scaled));
}


// fixed_t DoubleToFixed(const double num)
// {
//   const int32_t rounded = std::floor(num);
//
//   // extract the floating part
//   const long long tempFloatX = std::round((num - rounded) * 1e16);
//
//   const int16_t fracPart = (tempFloatX << (sizeof(tempFloatX) - FRAC_BITS)) >> (sizeof(tempFloatX) - FRAC_BITS);
//
//   return (rounded << FRAC_BITS) + fracPart;
// }


double FixedToDouble(const fixed_t num) { return static_cast<double>(num) / FRAC_UNIT; }