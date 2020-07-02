#ifndef __MIPS32_H__
#define __MIPS32_H__

#include "common_defs.h"
#include "bit_utils.h"
#include "mips32_defs.h"

class MIPS32_CP0 {
private:

    u32 index;
    u32 random;
    // u32 entrylo0;
    // u32 entrylo1;
    // u32 context;
    // u32 pagemask;
    u32 wired;
    u32 badvaddr;
    u32 count;
    // u32 entryhi;
    u32 compare;
    // u32 status;
    bool status_cu0;
    bool status_cu1;
    bool status_cu2;
    bool status_cu3;
    bool status_bev;
    u32  status_im;
    bool status_um;
    bool status_exl;
    bool status_erl;
    bool status_ie;
    // u32 cause;
    bool cause_bd;
    u32  cause_ce;
    bool cause_iv;
    u32  cause_ip;
    u32  cause_exccode; 
    u32  epc;
    // u32 prid;
    // u32 ebase;
    // u32 config;
    // u32 config1;
    // u32 taglo;
    // u32 taghi;
    u32 errorepc;

public:
    bool irq[6];

    MIPS32_CP0();
    void reset();
    void cycle();
    // bool* get_irq(u32 irq_num);
    u32  read(u32 reg);
    void write(u32 reg, u32 value);
    void exception(Exception e, u32 pc, u32 info, bool isStore, bool inDelaySlot);

    bool Status_CU(u32 cpnum);
    bool Status_BEV();
    bool Status_UM();
    bool Status_ERL();
    bool Status_EXL();
    bool Status_IE();
    bool Cause_IV();

    bool hasInterrupt();

};

#endif
