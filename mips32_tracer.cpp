#include "mips32_tracer.h"

MIPS32_Tracer::MIPS32_Tracer(string trace_file)
{
    trace_info.open(trace_file, ios::in);
    tracer_on = true;
}

MIPS32_Tracer::~MIPS32_Tracer()
{
    trace_info.close();
}

void MIPS32_Tracer::trigger(bool enable)
{
    tracer_on = enable;
}

void MIPS32_Tracer::trace(u32 pc, u32 wraddr, u32 wrdata)
{
    
    u32 temp;
    u32 trace_pc;
    u32 trace_wraddr;
    u32 trace_wrdata;

    if (!trace_info.eof()) {
        trace_info >> hex >> temp >> trace_pc >> trace_wraddr >> trace_wrdata;
    }
    else {
        return;
    }
    
    if(!tracer_on) return;
    
    if (pc != trace_pc || wraddr != trace_wraddr || wrdata != trace_wrdata) {
        cout << "--------------------------------------------------------------" << endl;
        cout << "Error!!!" << endl;
        cout << "    reference: PC = 0x" << hex << trace_pc
             << ", wb_rf_wnum = 0x" << hex << trace_wraddr
             << ", wb_rf_wdata = 0x" << hex << trace_wrdata << endl;
        cout << "    mycpu    : PC = 0x"<< hex << pc
             << ", wb_rf_wnum = 0x" << hex << wraddr
             << ", wb_rf_wdata = 0x" << hex << wrdata << endl;
        cout << "--------------------------------------------------------------" << endl;
        
        // Infinite Loop
        while(true);
    }
}
