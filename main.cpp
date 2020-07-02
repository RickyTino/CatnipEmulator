// #include "common_defs.h"
// #include "bit_utils.h"
#include "soc_lite.h"

// u32 clz(u32 a)
// {
//     for (u32 i = 0; i < 32; ++i) {
//         if (a & 0x80000000)
//             return i;
//         a <<= 1;
//     }
//     return 32;
// }

int main()
{
    // u32 a = 0xDEC0DE1C;
    // u32 t = bitPart(a, 27, 11);
    // cout << hex << t << endl;
    // u32 x1 = 0x12;
    // u32 x2 = bitConcat(x1, 8, 0x34);
    // u32 x3 = bitConcat(x2, 8, 0x56);
    // u32 x4 = bitConcat(x3, 8, 0x78);
    // cout << x4 << endl;
    // a = bitReplace(a, 23, 8, 0xDEAD);
    // cout <<  a << endl;

    // cout << CP0_INDEX << endl;
    // cout << BADVADDR << endl;

    // u32 b = 0x80000000;
    // cout << clz(b) << endl;

    // return 0;
    SoCLite soc;
    soc.run_perf();
    return 0;
}


