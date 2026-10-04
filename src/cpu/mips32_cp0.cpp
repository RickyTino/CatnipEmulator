#include "mips32_cp0.h"

MIPS32_CP0::MIPS32_CP0()
{
    reset();
}

void MIPS32_CP0::reset()
{
    tlb.reset();

    index_p      = false;
    index5       = 0;
    random_      = 31;
    wired        = 0;
    entrylo0     = 0;
    entrylo1     = 0;
    context      = 0;
    pagemask     = 0;
    badvaddr     = 0;
    entryhi      = 0;
    count33      = 0;
    compare      = 0;
    taglo        = 0;
    taghi        = 0;
    epc          = 0;
    errorepc     = 0xbfc00000;
    config_k0    = 2;

    status_cu0   = 0;
    status_bev   = 1;
    status_im    = 0;
    status_um    = 0;
    status_erl   = 1;
    status_exl   = 0;
    status_ie    = 0;

    cause_bd     = 0;
    cause_ce     = 0;
    cause_iv     = 0;
    cause_ip     = 0;
    cause_exccode = 0;
    timer_intr   = false;

    for (int i = 0; i < 6; ++i)
        irq[i] = false;
}

// PageMask[28:13] is stored as 16 bits, each bit duplicated into a bit-pair
// (MangoMIPS32 CP0.v "w_mask"): writes are folded so only legal runs of 1's
// survive.  Reads always return a consistent PageMask value.
static u32 foldPageMask(u32 wdata)
{
    u32 m = 0;
    for (u32 i = 0; i < 16; i += 2) {
        if (wdata & (1u << (13 + i)))
            m |= 3u << i;
    }
    return m;
}

void MIPS32_CP0::cycle()
{
    // Count increments every cycle; Count (the observable register) is the
    // inner counter >> 1, matching CP0.v's 33-bit Count__.
    count33 += 1;

    // Random decrements toward Wired, then wraps to 31.
    random_ = (random_ == wired) ? 31 : random_ - 1;

    // Timer interrupt latch: Count == Compare (Compare != 0 disables).
    u32 count = (u32)(count33 >> 1);
    if (compare != 0 && count == compare)
        timer_intr = true;

    // Refresh hardware interrupt pending bits; software bits [1:0] persist.
    // IP[7:2] = {irq[5] | timer_intr, irq[4], irq[3], irq[2], irq[1], irq[0]}.
    // Rebuilt in one expression (this runs every cycle).
    cause_ip = (cause_ip & ~0x000000FCu)
             | ((u32)irq[0] << 2)
             | ((u32)irq[1] << 3)
             | ((u32)irq[2] << 4)
             | ((u32)irq[3] << 5)
             | ((u32)irq[4] << 6)
             | ((u32)(irq[5] || timer_intr) << 7);
}

u32 MIPS32_CP0::read(u32 reg)
{
    u32 res = 0;
    switch (reg) {
        case CP0_INDEX:    res = bitSet(index5, 31, index_p); break;
        case CP0_RANDOM:   res = random_; break;
        case CP0_ENTRYLO0: res = entrylo0; break;
        case CP0_ENTRYLO1: res = entrylo1; break;
        case CP0_CONTEXT:  res = context; break;
        case CP0_PAGEMASK: res = pagemask << 13; break;
        case CP0_WIRED:    res = wired; break;
        case CP0_BADVADDR: res = badvaddr; break;
        case CP0_COUNT:    res = (u32)(count33 >> 1); break;
        case CP0_ENTRYHI:  res = entryhi & 0xFFFFE0FF; break;
        case CP0_COMPARE:  res = compare; break;

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

        case CP0_EPC:      res = epc; break;
        case CP0_PRID:     res = 0x00018000; break;    // Mango CP0.v PrId
        case CP0_CONFIG:   res = 0x80000080 | config_k0; break; // MT=001 TLB
        case CP0_CONFIG1:
            // Same fields as CP0.v's Config1: MMU size - 1 plus the I/D cache
            // line-size fields from Config.v (ICache_N = DCache_N = 2, so
            // IS = DS = 1).  This kernel's decode_config1() reads only
            // MD/PC/WR/CA/EP/FP/TLBS, so IS/DS are cosmetic - but 0 was a
            // silent deviation from the RTL.
            res = (31u << 25)       // 30:25 MMUSize-1 = 31 -> 32 entries
                | (1u  << 22)       // 24:22 IS = ICache_N - 1
                | (5u  << 19)       // 21:19 IL = 5 (64B)
                | (1u  << 13)       // 15:13 DS = DCache_N - 1
                | (5u  << 10);      // 12:10 DL = 5 (64B)
            break;
        case CP0_TAGLO:    res = taglo; break;
        case CP0_TAGHI:    res = taghi; break;
        case CP0_ERROREPC: res = errorepc; break;

        default: res = 0;
    }
    return res;
}

void MIPS32_CP0::write(u32 reg, u32 value)
{
    switch (reg) {
        case CP0_INDEX:    index5 = value & 0x1F; break;
        case CP0_ENTRYLO0: entrylo0 = value & 0x3FFFFFF; break;
        case CP0_ENTRYLO1: entrylo1 = value & 0x3FFFFFF; break;
        case CP0_CONTEXT:  context = (context & 0x007FFFFF) | (value & 0xFF800000); break;
        case CP0_PAGEMASK: pagemask = foldPageMask(value); break;
        case CP0_WIRED:
            wired = value & 0x1F;
            random_ = 31;
            break;
        case CP0_BADVADDR: badvaddr = value; break;
        case CP0_COUNT:    count33 = (u64)value << 1; break;
        case CP0_ENTRYHI:  entryhi = value & 0xFFFFE0FF; break;
        case CP0_COMPARE:
            compare = value;
            timer_intr = false;
            break;

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

        case CP0_EPC:      epc = value; break;
        case CP0_CONFIG:   config_k0 = value & 0x7; break;
        case CP0_TAGLO:    taglo = value; break;
        case CP0_TAGHI:    taghi = value; break;
        case CP0_ERROREPC: errorepc = value; break;

        default: break;
    }
}

void MIPS32_CP0::tlbwi()
{
    u32 hi = entryhi & 0xFFFFE0FF;
    tlb.writeEntry(index5, pagemask << 13, hi, entrylo0, entrylo1);
}

void MIPS32_CP0::tlbwr()
{
    u32 hi = entryhi & 0xFFFFE0FF;
    tlb.writeEntry(random_, pagemask << 13, hi, entrylo0, entrylo1);
}

void MIPS32_CP0::tlbr()
{
    const MIPS32_TLBItem &it = tlb.get(index5);
    pagemask = it.mask;
    entryhi  = (it.vpn2 << 13) | (it.asid & 0xFF);
    entrylo0 = (it.pfn0 << 6) | (it.c0 << 3) | (it.d0 << 2) | (it.v0 << 1) | (u32)it.g;
    entrylo1 = (it.pfn1 << 6) | (it.c1 << 3) | (it.d1 << 2) | (it.v1 << 1) | (u32)it.g;
}

void MIPS32_CP0::tlbp()
{
    int idx = tlb.probe((entryhi >> 13) & 0x7FFFF, entryhi & 0xFF);
    if (idx < 0) {
        index_p = true;
        index5  = 0;
    }
    else {
        index_p = false;
        index5  = (u32)idx;
    }
}

bool MIPS32_CP0::tlbTranslate(u32 vaddr, u32 &pfn, bool &vld, bool &drt,
                              u32 &cat, u32 &eob)
{
    return tlb.translate(vaddr, entryhi & 0xFF, pfn, vld, drt, cat, eob);
}

void MIPS32_CP0::exception(Exception e, u32 pc, u32 info, bool isStore, bool inDelaySlot)
{
    if (e == ERET) {
        // Return from exception: ERL takes priority (kernel mode entry);
        // otherwise leave kernel exception level.
        // NOTE: MangoMIPS32 CP0.v clears EXL only and leaves ERL set here,
        // which is a known RTL bug (MangoMIPS32/KnownBugs.md BUG-001).
        // This implementation follows the MIPS32 spec instead - do not
        // "fix" it to match the RTL.
        if (status_erl)
            status_erl = false;
        else
            status_exl = false;
    }
    else {
        if (!status_exl) {
            epc      = inDelaySlot ? pc - 4 : pc;
            cause_bd = inDelaySlot;
        }
        status_exl = true;
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
            // Update BadVAddr / Context.BadVPN2 / EntryHi.VPN2 (CP0.v).
            badvaddr = info;
            context  = (context & ~0x007FFFF0u) | ((info >> 13) << 4);
            entryhi  = (entryhi & ~0xFFFFE000u) | (info & 0xFFFFE000u);
            break;

        case CP_UNUSABLE:
            cause_ce = info;
            break;

        default:
            break;
    }

    switch (e) {
        case INTERRUPT:   cause_exccode = EXCCODE_INT; break;
        case I_ADE:       cause_exccode = EXCCODE_ADEL; break;
        case I_TLBR:      cause_exccode = EXCCODE_TLBL; break;
        case I_TLBI:      cause_exccode = EXCCODE_TLBL; break;
        case CP_UNUSABLE: cause_exccode = EXCCODE_CPU; break;
        case RESVINST:    cause_exccode = EXCCODE_RI; break;
        case INTOVERFLOW: cause_exccode = EXCCODE_OV; break;
        case TRAP:        cause_exccode = EXCCODE_TR; break;
        case SYSCALL:     cause_exccode = EXCCODE_SYS; break;
        case BREAKPOINT:  cause_exccode = EXCCODE_BP; break;
        case D_ADE:       cause_exccode = isStore ? EXCCODE_ADES : EXCCODE_ADEL; break;
        case D_TLBR:      cause_exccode = isStore ? EXCCODE_TLBS : EXCCODE_TLBL; break;
        case D_TLBI:      cause_exccode = isStore ? EXCCODE_TLBS : EXCCODE_TLBL; break;
        case D_TLBM:      cause_exccode = EXCCODE_MOD; break;
        default: break;
    }
}

bool MIPS32_CP0::Status_CU(u32 cpnum)
{
    if (cpnum == 0) return status_cu0;
    return false;
}

bool MIPS32_CP0::hasInterrupt()
{
    if ((cause_ip & status_im) == 0) return false;
    if (!status_ie || status_exl || status_erl) return false;
    return true;
}
