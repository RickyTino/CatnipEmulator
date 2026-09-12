#ifndef __MIPS32_CP0_H__
#define __MIPS32_CP0_H__

#include "common_defs.h"
#include "bit_utils.h"
#include "mips32_defs.h"
#include "mips32_tlb.h"

// Behavior reference: MangoMIPS32 Src/Core/CP0.v
// 20 MIPS32 Release 1 CP0 registers; the 32-entry TLB is owned here.
class MIPS32_CP0 {
private:
    // --- TLB ---
    MIPS32_TLB tlb;

    // --- Registers (fields kept in the same layout as CP0.v wires) ---
    bool    index_p;           // Index.P
    u32     index5;            // Index[4:0]
    u32     random_;           // Random, maintained by cycle()
    u32     wired;             // Wired[4:0]
    u32     entrylo0;          // [25:6] PFN [5:3] C D V G (low 26 bits)
    u32     entrylo1;
    u32     context;           // [31:23] PTEBase [22:4] BadVPN2 [3:0]=0
    u32     pagemask;          // [15:0], PageMask[28:13] contents
    u32     badvaddr;
    u32     entryhi;           // [31:13] VPN2 [7:0] ASID ([12:8] read as 0)
    u64     count33;           // inner counter; Count = count33>>1
    u32     compare;
    u32     taglo;
    u32     taghi;
    u32     epc;
    u32     errorepc;
    u32     config_k0;         // Config[2:0] (K23/KU read 0 in TLB mode)

    // --- Status fields ---
    bool    status_cu0;
    bool    status_bev;
    u32     status_im;         // [15:8]
    bool    status_um;
    bool    status_erl;
    bool    status_exl;
    bool    status_ie;

    // --- Cause fields ---
    bool    cause_bd;
    u32     cause_ce;
    bool    cause_iv;
    u32     cause_ip;          // [7:0], refreshed every cycle (soft bits kept)
    u32     cause_exccode;

    bool    timer_intr;        // Count == Compare latch

public:
    // Hardware interrupt input lines: irq[i] -> Cause.IP[i+2].
    // IP[7] additionally ORs the internal timer interrupt (like CP0.v).
    bool irq[6];

    MIPS32_CP0();
    void reset();
    void cycle();
    u32  read(u32 reg);
    void write(u32 reg, u32 value);
    void exception(Exception e, u32 pc, u32 info, bool isStore, bool inDelaySlot);

    // --- Status helpers used by the core ---
    bool Status_CU(u32 cpnum);
    bool Status_BEV() { return status_bev; }
    bool Status_UM()  { return status_um;  }
    bool Status_ERL() { return status_erl; }
    bool Status_EXL() { return status_exl; }
    bool Status_IE()  { return status_ie;  }
    bool Cause_IV()   { return cause_iv;   }

    bool hasInterrupt();

    // --- TLB instructions / translation (TLBU.v semantics) ---
    void tlbwi();            // write entry Index[4:0]
    void tlbwr();            // write entry Random
    void tlbr();             // read entry Index[4:0] back into the regs
    void tlbp();             // probe EntryHi; sets Index or Index.P

    // Translation used by the MMU (core).  ASID comes from EntryHi.
    // Returns false on no-match (TLB Refill); otherwise fills the fields of
    // the matched half page (V/D are reported; enforced by the core).
    bool tlbTranslate(u32 vaddr, u32 &pfn, bool &vld, bool &drt,
                      u32 &cat, u32 &eob);
};

#endif
