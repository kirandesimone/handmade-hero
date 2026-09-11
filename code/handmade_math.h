#ifndef HANDMADE_MATH_H
#define HANDMADE_MATH_H

#include <math.h>
#include <cstdint>

inline int32_t
floor_float(float value)
{
    // change to something better
    return (int32_t)floorf(value);
}

#endif // HANDMADE_MATH_H
