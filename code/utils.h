#ifndef __UTILS_H__
#define __UTILS_H__

#include "core.h"

// HELPER FUNCTIONS
// -----------------


// INLINE FUNCTIONS
// ----------------

inline float sign(float x)
{
    return (x > 0.0f) - (x < 0.0f);
}

inline uint32_t roundUp(uint32_t value, uint32_t multiple)
{
    return (value + multiple - 1) & ~(multiple - 1);
}


#endif // __UTILS_H__
