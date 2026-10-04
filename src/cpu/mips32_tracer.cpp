#include "mips32_tracer.h"

MIPS32_Tracer::MIPS32_Tracer(string trace_file)
{
    trace_info.open(trace_file, ios::in);
    // Without the reference trace every comparison is silently skipped, which
    // is indistinguishable from a passing test - so say it out loud.
    if (!trace_info.is_open())
        cerr << EMU_TAG << "warning: cannot open golden trace '" << trace_file
             << "'; the functional test will not be checked" << endl;
    tracer_on = true;
    records = 0;
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

    // Stop once the reference trace runs out.  Test for a successful extraction
    // rather than eof(): after the last record the stream may still hold a
    // trailing newline, and eof() only becomes true after a failed read - so
    // eof() alone would let one bogus record through and report a false
    // mismatch.  Records are still consumed while the comparison is off, so the
    // file stays aligned with the retired-instruction count.
    if (!(trace_info >> hex >> temp >> trace_pc >> trace_wraddr >> trace_wrdata))
        return;

    ++records;

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

        // Fail instead of spinning: from the outside a hang looks exactly like
        // a slow test, and the record index is the quickest way to the
        // offending instruction.
        cerr << EMU_TAG << "trace mismatch at record " << dec << records
             << " (mycpu PC = 0x" << hex << pc << ")" << endl;
        exit(1);
    }
}
