#include "bit_utils.h"

// bitPart: Verilog " num[msb:lsb] ".
u32 bitPart(u32 a, u32 msb, u32 lsb)
{
    if (msb < lsb)
        return 0;
    return ((a >> lsb) & ((1 << (msb - lsb + 1)) - 1));
}

// bitConcat: Verilog " {a, b[b_width-1:0]}[31:0] ".
u32 bitConcat(u32 a, u32 b_width, u32 b)
{
    if (b_width >= 32)
        return b;
    return (b & ((1 << b_width) - 1)) | (a << b_width);
}

// bitReplace: Verilog " a[msb:lsb] = b " (return value of a)
u32 bitReplace(u32 a, u32 msb, u32 lsb, u32 b)
{
    if (msb < lsb)
        return a;
    return bitConcat(bitPart(a, 31, msb + 1), msb + 1, bitConcat(b, lsb, a));
}

// bitSet: Verilog " a[index] = b " (return value of a)
u32 bitSet(u32 a, u32 index, bool bit)
{
    return bit ? SETBIT(a, index) : CLRBIT(a, index);
}