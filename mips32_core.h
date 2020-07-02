#ifndef __MIPS32_CORE_H__
#define __MIPS32_CORE_H__

#include "common_defs.h"
#include "bit_utils.h"
#include "axi.h"
#include "mips32_defs.h"
#include "mips32_cp0.h"
#include "mips32_tracer.h"

class MIPS32_Core {
private:
    
    MIPS32_CP0 cp0;
    MIPS32_Tracer *tracer;
    AXI32_Slave *memory;

    u32  pc;
    u32  nextpc;
    u32  inst;
    u32  gpr[32];
    u64  hilo;
    u32  &hi = *((u32*)&hilo + 1);
    u32  &lo = *((u32*)&hilo);
    bool llbit;
    bool branch_flag;
    bool branch_taken;
    bool clearDelaySlot;
    u32  branch_addr;
    bool inDelaySlot;
    bool exception_flag;
    Exception exception_type;


    
private:
    void reset();
    u32  fetch(u32 addr);
    void execute(u32 inst);

    void exception(Exception e, u32 info = 0, bool save = false);
    // void branch(bool cond, bool link, bool likely, u32 addr);
    void branch(bool cond, u32 addr, bool likely, u32 link_reg = 0);
    // void branch_link(bool cond, bool likely, u32 addr, u32 link_reg);
    void renewpc();

    void writeGPR(u32 regaddr, u32 data);  // Don't write gpr[0]
    u32  addrTranslate(u32 addr);
    bool addrCCA(u32 vaddr);
    void load(u32 reg, u32 vaddr, u32 size, bool isSigned);
    void store(u32 reg, u32 vaddr, u32 size);
    void load_ual(u32 reg, u32 vaddr, bool left);
    void store_ual(u32 reg, u32 vaddr, bool left);
    void load_link(u32 reg, u32 vaddr);
    void store_cond(u32 reg, u32 vaddr);
    // void multiply(u32 rs, u32 rt, bool isSigned);
    void divide(u32 rs, u32 rt, bool isSigned);
    // bool intOverflow(s32 a, s32 b);
    void add_CheckOv(s32 a, s32 b, u32 rd);
    u32  clz(u32 a);

    bool usermode();
    bool user_exc(u32 addr);
    bool aligned(u32 a, u32 align);
    
public:
    MIPS32_Core(AXI32_Slave *mem);
    MIPS32_Core(AXI32_Slave *mem, MIPS32_Tracer *t);
    void run();
    void cycle();
    bool* get_irq(u32 irq_num);
    
};

#endif
