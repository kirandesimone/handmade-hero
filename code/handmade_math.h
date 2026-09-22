#ifndef HANDMADE_MATH_H
#define HANDMADE_MATH_H

#include <cmath>
#include <cstdint>

inline int32_t
round_float(float value)
{
    return (int32_t)roundf(value);
}

inline int32_t
floor_float(float value)
{
    // change to something better
    return (int32_t)floorf(value);
}

#endif // HANDMADE_MATH_H
