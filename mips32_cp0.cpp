#include "mips32_cp0.h"

MIPS32_CP0::MIPS32_CP0() {

    // wmask[CP0_INDEX   ] = 0x0000001F;
    // wmask[CP0_RANDOM  ] = 0x0000001F;
    // wmask[ENTRYLO0] = 0x03FFFFFF;
    // wmask[ENTRYLO1] = 0x03FFFFFF;
    // wmask[CONTEXT ] = 0xFF800000;
    // wmask[PAGEMASK] = 0x5;
    // wmask[WIRED   ] = 0x6;
    // wmask[BADVADDR] = 0x8;
    // wmask[COUNT   ] = 0x9;
    // wmask[ENTRYHI ] = 0x10;
    // wmask[COMPARE ] = 0x11;
    // wmask[STATUS  ] = 0x12;
    // wmask[CAUSE   ] = 0x13;
    // wmask[EPC     ] = 0x14;
    // wmask[PRID    ] = 0x15;
    // wmask[EBASE   ] = 0x15;
    // wmask[CONFIG  ] = 0x16;
    // wmask[CONFIG1 ] = 0x16;
    // wmask[TAGLO   ] = 0x28;
    // wmask[TAGHI   ] = 0x29;
    // wmask[ERROREPC] = 0x30;
    reset();
}

void MIPS32_CP0::reset()
{
    // index = 0;
    // random = 31;
    // wired = 0;
    badvaddr      = 0;
    count         = 0;
    compare       = 0;
    status_cu0    = 0;
    status_bev    = 1;
    status_im     = 0;
    status_um     = 0;
    status_erl    = 1;
    status_exl    = 0;
    status_ie     = 0;
    cause_bd      = 0;
    cause_ce      = 0;
    cause_iv      = 0;
    cause_ip      = 0;
    cause_exccode = 0;
    epc           = 0;
    
    irq[0]        = 0;
    irq[1]        = 0;
    irq[2]        = 0;
    irq[3]        = 0;
    irq[4]        = 0;
    irq[5]        = 0;
}

void MIPS32_CP0::cycle() {
    count = count + 1;
    if (compare != 0 && count == compare) {
        irq[5] = true;
    }
    random = (random == wired) ? 31 : random - 1;
    for (int i = 0; i < 6; ++i) 
        cause_ip = bitSet(cause_ip, i + 2, irq[i]);
}

// bool* MIPS32_CP0::get_irq(u32 irq_num)
// {
//     if(irq_num > 7) return NULL;
//     return &(irq[irq_num - 2]);
// }

u32 MIPS32_CP0::read(u32 reg)
{
    u32 res = 0;
    switch (reg) {
        case CP0_COUNT:    res = count; break;
        case CP0_COMPARE:  res = compare; break;
        case CP0_BADVADDR: res = badvaddr; break;
        case CP0_EPC:      res = epc; break;
        case CP0_STATUS:
            res = bitSet(res, 28, status_cu0);
            res = bitSet(res, 22, status_bev);
            res = bitReplace(res, 15, 8, status_im);
            res = bitSet(res,  4, status_um);
            res = bitSet(res,  2, status_erl);
            res = bitSet(res,  1, status_exl);
            res = bitSet(res,  0, status_ie);
            break;
        
        case CP0_CAUSE:
            res = bitSet(res, 31, cause_bd);
            res = bitReplace(res, 29, 28, cause_ce);
            res = bitSet(res, 23, cause_iv);
            res = bitReplace(res, 15, 8, cause_ip);
            res = bitReplace(res,  6, 2, cause_exccode);
            break;
        
        default: res = 0;
    }
    return res;
}

void MIPS32_CP0::write(u32 reg, u32 value)
{
    switch (reg) {
        case CP0_COUNT:    count = value; break;
        case CP0_COMPARE:  compare = value; break;
        case CP0_EPC:      epc = value; break;
        case CP0_BADVADDR: badvaddr = value; break;
        case CP0_STATUS:
            status_cu0 = GETBIT(value, 28);
            status_bev = GETBIT(value, 22);
            status_im  = bitPart(value, 15, 8);
            status_um  = GETBIT(value,  4);
            status_erl = GETBIT(value,  2);
            status_exl = GETBIT(value,  1);
            status_ie  = GETBIT(value,  0);
            break;
        
        case CP0_CAUSE:
            cause_iv = GETBIT(value, 23);
            cause_ip = bitReplace(cause_ip, 1, 0, bitPart(value, 9, 8));
            break;
        
        default: break;
    }
}

void MIPS32_CP0::exception(Exception e, u32 pc, u32 info, bool isStore, bool inDelaySlot)
{
    // Deal with ERET
    if (e == ERET) {
        status_exl = 0;
    }
    else {
        if(!status_exl){
            epc = inDelaySlot ? pc - 4 : pc;
            cause_bd = inDelaySlot;
        }
        status_exl = 1;
    }

    switch (e) {
        case I_ADE:
        case D_ADE:
            badvaddr = info;
            break;
        case I_TLBR:
        case I_TLBI:
        case D_TLBR:
        case D_TLBI:
        case D_TLBM:
            badvaddr = info;
            // context_badvpn2 = ?
            // entryhi_vpn2 = 
            break;
        
        case CP_UNUSABLE:
            cause_ce = info;
            break;
    }

    switch (e) {
        case INTERRUPT: cause_exccode = EXCCODE_INT; break;
        case I_ADE: cause_exccode = EXCCODE_ADEL; break;
        case I_TLBR: cause_exccode = EXCCODE_TLBL; break;
        case I_TLBI: cause_exccode = EXCCODE_TLBL; break;
        case CP_UNUSABLE: cause_exccode = EXCCODE_CPU; break;
        case RESVINST: cause_exccode = EXCCODE_RI; break;
        case INTOVERFLOW: cause_exccode = EXCCODE_OV; break;
        case TRAP: cause_exccode = EXCCODE_TR; break;
        case SYSCALL: cause_exccode = EXCCODE_SYS; break;
        case BREAKPOINT: cause_exccode = EXCCODE_BP; break;
        case D_ADE: cause_exccode = isStore ? EXCCODE_ADES : EXCCODE_ADEL; break;
        case D_TLBR: cause_exccode = isStore ? EXCCODE_TLBS : EXCCODE_TLBL; break;
        case D_TLBI: cause_exccode = isStore ? EXCCODE_TLBS : EXCCODE_TLBL; break;
        case D_TLBM: cause_exccode = EXCCODE_MOD; break;
    }
    
//    cout << "Cause.ExcCode = " << cause_exccode << endl;
}

bool MIPS32_CP0::Status_CU(u32 cpnum)
{
    if (cpnum == 0) return status_cu0;
    // if(cpnum == 1) return status_cu1;
    // if(cpnum == 2) return status_cu2;
    // if(cpnum == 3) return status_cu3;
    return 0;
}

bool MIPS32_CP0::Status_BEV() { return status_bev; }
bool MIPS32_CP0::Status_UM()  { return status_um;  }
bool MIPS32_CP0::Status_ERL() { return status_erl; }
bool MIPS32_CP0::Status_EXL() { return status_exl; }
bool MIPS32_CP0::Status_IE()  { return status_ie;  }
bool MIPS32_CP0::Cause_IV()   { return cause_iv;   }

bool MIPS32_CP0::hasInterrupt()
{
    if ((cause_ip & status_im) == 0) return false;
    if (!status_ie || status_exl || status_erl) return false;
    return true;
}
