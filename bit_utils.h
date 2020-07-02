#ifndef __BIT_UTILS_H__
#define __BIT_UTILS_H__

#include "common_defs.h"

// Basic Bit Operations
#define SETBIT(x,y)     x |= (1 << y)
#define CLRBIT(x,y)     x &= ~(1 << y)
#define REVBIT(x,y)     x ^= (1 << y)
#define GETBIT(x,y)     ((x) >> (y) & 1)

// Bit functions
u32 bitPart(u32 a, u32 msb, u32 lsb);
u32 bitConcat(u32 a, u32 b_width, u32 b);
u32 bitReplace(u32 a, u32 msb, u32 lsb, u32 b);
u32 bitSet(u32 a, u32 index, bool bit);

#endif