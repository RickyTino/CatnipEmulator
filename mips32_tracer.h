#ifndef __MIPS32_TRACER_H__
#define __MIPS32_TRACER_H__

#include "common_defs.h"
// #include <fstream>

class MIPS32_Tracer {
private:
    ifstream trace_info;
    bool tracer_on;

public:
    MIPS32_Tracer(string trace_file);
    ~MIPS32_Tracer();
    void trigger(bool enable);
    void trace(u32 pc, u32 wraddr, u32 wrdata);
};

#endif
