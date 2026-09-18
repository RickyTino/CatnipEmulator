#ifndef __BIT_UTILS_H__
#define __BIT_UTILS_H__

#include "common_defs.h"

// Basic Bit Operations
#define SETBIT(x,y)     x |= (1 << y)
#define CLRBIT(x,y)     x &= ~(1 << y)
#define REVBIT(x,y)     x ^= (1 << y)
#define GETBIT(x,y)     ((x) >> (y) & 1)

// Bit functions.  These sit on the interpreter's hot path (dozens of calls per
// emulated instruction), so they are inline; with constant arguments the
// compiler folds them to a shift + mask.
// bitPart: Verilog " num[msb:lsb] ".
inline u32 bitPart(u32 a, u32 msb, u32 lsb)
{
    if (msb < lsb)
        return 0;
    u32 width = msb - lsb + 1;
    if (width >= 32)        // (a >> lsb) is already 32 bits wide
        return a >> lsb;
    return (a >> lsb) & ((1u << width) - 1);
}

// bitConcat: Verilog " {a, b[b_width-1:0]}[31:0] ".
inline u32 bitConcat(u32 a, u32 b_width, u32 b)
{
    if (b_width >= 32)
        return b;
    return (b & ((1u << b_width) - 1)) | (a << b_width);
}

// bitReplace: Verilog " a[msb:lsb] = b " (return value of a)
inline u32 bitReplace(u32 a, u32 msb, u32 lsb, u32 b)
{
    if (msb < lsb)
        return a;
    return bitConcat(bitPart(a, 31, msb + 1), msb + 1, bitConcat(b, lsb, a));
}

// bitSet: Verilog " a[index] = b " (return value of a)
inline u32 bitSet(u32 a, u32 index, bool bit)
{
    return bit ? SETBIT(a, index) : CLRBIT(a, index);
}

#endif
