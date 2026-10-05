#include "mips32_core.h"

// Instruction / exception trace.  Kept for debugging but compiled out by
// default so it adds nothing to the hot path: set CEMU_TRACE to 1 and rebuild
// to bring it back (with CEMU_TRACE set in the environment, 1 traces every
// instruction and 2 samples the PC every 2^20 cycles).
#define CEMU_TRACE 0

#if CEMU_TRACE
static int cemuTrace()
{
    static int mode = -2;
    if (mode == -2) {
        const char *e = getenv("CEMU_TRACE");
        mode = e ? atoi(e) : 0;
    }
    return mode;
}

static u64 g_cycles = 0;
#endif

MIPS32_Core::MIPS32_Core(AXI32_Slave *mem)
{
    memory = mem;
    tracer = NULL;
    reset();
}

MIPS32_Core::MIPS32_Core(AXI32_Slave *mem, MIPS32_Tracer *t)
{
    memory = mem;
    tracer = t;
    reset();
}

void MIPS32_Core::reset()
{
    nextpc = 0xbfc00000;
    pc = 0;
    for (u32 i = 0; i < 32; ++i)
        gpr[i] = 0;
    hi = 0;
    lo = 0;
    llbit = false;
    branch_flag = false;
    branch_taken = false;
    inDelaySlot = false;
    exception_flag = false;
    cp0.reset();
}

void MIPS32_Core::setEntry(u32 entry)
{
    // renewpc() starts each cycle with pc = nextpc, so parking both here lets
    // the first cycle fetch from `entry`.  CP0 keeps its reset state.
    pc = entry;
    nextpc = entry;
    branch_flag = false;
    branch_taken = false;
    inDelaySlot = false;
    exception_flag = false;
}

void MIPS32_Core::run()
{
    while(1) {
        cycle();
    }
}

void MIPS32_Core::cycle()
{
    // Initialize cycle
    exception_flag = false;

    // Renew PC
    renewpc();

    // CP0 cycle
    cp0.cycle();

    // Check interrupt
    if (cp0.hasInterrupt()) 
        exception(INTERRUPT);

    // Inst fetch
    inst = fetch(pc);

#if CEMU_TRACE
    {
        int tm = cemuTrace();
        if (tm == 1) {
            fprintf(stderr, "pc=%08x inst=%08x\n", pc, inst);
        } else if (tm >= 2) {
            ++g_cycles;
            if ((g_cycles & 0xFFFFF) == 0)
                fprintf(stderr, "[cyc %llu pc=%08x]\n",
                        (unsigned long long)g_cycles, pc);
        }
    }
#endif

    // Execute
    execute(inst);
}

bool* MIPS32_Core::get_irq(u32 irq_num)
{
    // irq_num is the interrupt line as the SoC names it (CP0 Cause.IPn), i.e.
    // 2..7 map onto irq[0..5].  The lower bound has to be checked as well:
    // with an unsigned irq_num, 0/1 would wrap in the subtraction below and
    // hand out a pointer outside cp0.irq[].
    if (irq_num < 2 || irq_num > 7) return NULL;
    return &(cp0.irq[irq_num - 2]);
}

u32 MIPS32_Core::fetch(u32 addr)
{
    if (exception_flag) return 0;
    if ((addr & 0x3) != 0) {
        exception(I_ADE, addr);
        return 0;
    }
    if (user_exc(addr)) {           // user mode fetch into kernel segments
        exception(I_ADE, addr);
        return 0;
    }
    u32 phyAddr;
    if (!addrTranslate(addr, false, true, phyAddr))
        return 0;
    return memory->read(phyAddr, 4);
}

void MIPS32_Core::execute(u32 inst)
{

    if (exception_flag) return;
    if (inst == 0) return;

    u32 opcode   = bitPart(inst, 31, 26);
    u32 rs       = bitPart(inst, 25, 21);
    u32 rt       = bitPart(inst, 20, 16);
    u32 rd       = bitPart(inst, 15, 11);
    u32 sa       = bitPart(inst, 10,  6);
    u32 funct    = bitPart(inst,  5,  0);
    u16 imme     = bitPart(inst, 15,  0);
    u32 uimme    = (u16)imme;
    s32 simme    = (s16)imme;
    u32 j_offset = bitPart(inst, 25,  0);
    u32 sel      = bitPart(inst,  2,  0);

    // Targets and the load/store effective address are formed in u32
    // arithmetic: shifting a negative s32 (simme << 2) or adding two signed
    // values that overflow is UB in C++, and this runs for every branch and
    // every memory access.
    u32 b_target = pc + ((u32)simme << 2) + 4;
    u32 j_target = (pc & 0xF0000000) | (j_offset << 2);
    u32 vaddr    = gpr[rs] + (u32)simme;

    switch (opcode) {
        case OP_SPECIAL:
            switch (funct) {
                case SP_SLL:     writeGPR(rd, gpr[rt] << sa); break;
                case SP_SRL:     writeGPR(rd, gpr[rt] >> sa); break;
                case SP_SRA:     writeGPR(rd, (s32)gpr[rt] >> sa); break;
                case SP_SLLV:    writeGPR(rd, gpr[rt] << (gpr[rs] & 0x1F)); break;
                case SP_SRLV:    writeGPR(rd, gpr[rt] >> (gpr[rs] & 0x1F)); break;
                case SP_SRAV:    writeGPR(rd, (s32)gpr[rt] >> (gpr[rs] & 0x1F)); break;
                case SP_JR:      branch(true, gpr[rs], 0); break;
                case SP_JALR:    branch(true, gpr[rs], 0, rd); break;
                case SP_MOVZ:    if (gpr[rt] == 0) writeGPR(rd, gpr[rs]); break;
                case SP_MOVN:    if (gpr[rt] != 0) writeGPR(rd, gpr[rs]); break;
                case SP_SYSCALL: exception(SYSCALL); break;
                case SP_BREAK:   exception(BREAKPOINT); break;
                case SP_SYNC:    break;
                case SP_MFHI:    writeGPR(rd, hi); break;
                case SP_MTHI:    hi = gpr[rs]; break;
                case SP_MFLO:    writeGPR(rd, lo); break;
                case SP_MTLO:    lo = gpr[rs]; break;
                case SP_MULT:    setHilo((u64)((s64)(s32)gpr[rs] * (s64)(s32)gpr[rt])); break;
                case SP_MULTU:   setHilo((u64)gpr[rs] * (u64)gpr[rt]); break;
                case SP_DIV:     divide(rs, rt, true); break;
                case SP_DIVU:    divide(rs, rt, false); break;
                case SP_ADD:     add_CheckOv(gpr[rs], gpr[rt], rd); break;
                case SP_ADDU:    writeGPR(rd, gpr[rs] + gpr[rt]); break;
                case SP_SUB:     sub_CheckOv(gpr[rs], gpr[rt], rd); break;
                case SP_SUBU:    writeGPR(rd, gpr[rs] - gpr[rt]); break;
                case SP_AND:     writeGPR(rd, gpr[rs] & gpr[rt]); break;
                case SP_OR:      writeGPR(rd, gpr[rs] | gpr[rt]); break;
                case SP_XOR:     writeGPR(rd, gpr[rs] ^ gpr[rt]); break;
                case SP_NOR:     writeGPR(rd, ~(gpr[rs] | gpr[rt])); break;
                case SP_SLT:     writeGPR(rd, (s32)gpr[rs] < (s32)gpr[rt]); break;
                case SP_SLTU:    writeGPR(rd, gpr[rs] < gpr[rt]); break;
                case SP_TGE:     if ((s32)gpr[rs] >= (s32)gpr[rt]) exception(TRAP); break;
                case SP_TGEU:    if (gpr[rs] >= gpr[rt]) exception(TRAP);break;
                case SP_TLT:     if ((s32)gpr[rs] < (s32)gpr[rt]) exception(TRAP);break;
                case SP_TLTU:    if (gpr[rs] <  gpr[rt]) exception(TRAP);break;
                case SP_TEQ:     if (gpr[rs] == gpr[rt]) exception(TRAP);break;
                case SP_TNE:     if (gpr[rs] != gpr[rt]) exception(TRAP);break;
                default:         exception(RESVINST); break;
            }
            break;

        case OP_REGIMM:
            switch (rt) {
                case RGI_BLTZ:    branch((s32)gpr[rs] <  0, b_target, 0); break;
                case RGI_BGEZ:    branch((s32)gpr[rs] >= 0, b_target, 0); break;
                case RGI_BLTZL:   branch((s32)gpr[rs] <  0, b_target, 1); break;
                case RGI_BGEZL:   branch((s32)gpr[rs] >= 0, b_target, 1); break;
                case RGI_TGEI:    if ((s32)gpr[rs] >= simme) exception(TRAP); break;
                case RGI_TGEIU:   if (gpr[rs] >= (u32)simme) exception(TRAP); break;
                case RGI_TLTI:    if ((s32)gpr[rs] <  simme) exception(TRAP); break;
                case RGI_TLTIU:   if (gpr[rs] <  (u32)simme) exception(TRAP); break;
                case RGI_TEQI:    if (gpr[rs] == (u32)simme) exception(TRAP); break;
                case RGI_TNEI:    if (gpr[rs] != (u32)simme) exception(TRAP); break;
                case RGI_BLTZAL:  branch((s32)gpr[rs] <  0, b_target, 0, 31); break;
                case RGI_BGEZAL:  branch((s32)gpr[rs] >= 0, b_target, 0, 31); break;
                case RGI_BLTZALL: branch((s32)gpr[rs] <  0, b_target, 1, 31); break;
                case RGI_BGEZALL: branch((s32)gpr[rs] >= 0, b_target, 1, 31); break;
            }
            break;

        case OP_J:     branch(true, j_target, 0); break;
        case OP_JAL:   branch(true, j_target, 0, 31); break;
        case OP_BEQ:   branch(gpr[rs] == gpr[rt], b_target, 0); break;
        case OP_BNE:   branch(gpr[rs] != gpr[rt], b_target, 0); break;
        case OP_BLEZ:  branch((s32)gpr[rs] <= 0, b_target, 0); break;
        case OP_BGTZ:  branch((s32)gpr[rs] >  0, b_target, 0); break;
        case OP_ADDI:  add_CheckOv(gpr[rs], simme, rt); break;
        case OP_ADDIU: writeGPR(rt, gpr[rs] + simme); break;
        case OP_SLTI:  writeGPR(rt, (s32)gpr[rs] < simme); break;
        case OP_SLTIU: writeGPR(rt, gpr[rs] < (u32)simme); break;
        case OP_ANDI:  writeGPR(rt, gpr[rs] & uimme); break;
        case OP_ORI:   writeGPR(rt, gpr[rs] | uimme); break;
        case OP_XORI:  writeGPR(rt, gpr[rs] ^ uimme); break;
        case OP_LUI:   writeGPR(rt, uimme << 16); break;
        
        case OP_COP0:
            if (usermode() && !cp0.Status_CU(0)) {
                exception(CP_UNUSABLE, 0);
            }
            else {
                switch (rs) {
                    case C0_MFC0:  writeGPR(rt, cp0.read(CP0_REG(rd, sel))); break;
                    case C0_MTC0:  cp0.write(CP0_REG(rd, sel), gpr[rt]); break;
                    case C0_CO:
                        switch (funct) {
                            case C0F_TLBR:  cp0.tlbr(); break;
                            case C0F_TLBWI: cp0.tlbwi(); break;
                            case C0F_TLBWR: cp0.tlbwr(); break;
                            case C0F_TLBP:  cp0.tlbp(); break;
                            case C0F_ERET: exception(ERET); break;
                            case C0F_WAIT: break;
                            default: exception(RESVINST); break;
                        }
                        break;
                    default: exception(RESVINST); break;
                }
            }
            break;

        // CpU, not RI: there is no CP1 on CatnipSoC and Status.CU1 always
        // reads 0, so every CP1-referencing instruction is a Coprocessor
        // Unusable one.  Vol III 6.2.22 lists "COP1, COP1X, LWC1, SWC1, LDC1,
        // SDC1 or MOVCI" in a single breath, and 6.1 has CpU outrank RI when
        // both apply to the same instruction.  The RTL agrees (Decode.v raises
        // exc_cpu; Exception.v's casez orders CpU before RI).
        //
        // The golden trace used to disagree: its reference core answered RI
        // for a COP1 word inside n76_ri_ex.S's reserved-instruction test, so
        // matching the trace meant being non-conformant here.  That word has
        // been replaced with a genuinely reserved encoding, and the trace and
        // the spec now agree - see the note in tb/soft/func/inst/n76_ri_ex.S.
        case OP_COP1:  exception(CP_UNUSABLE, 1); break;
        case OP_COP2:  exception(CP_UNUSABLE, 2); break;
        case OP_COP3:  exception(CP_UNUSABLE, 3); break;
        case OP_BEQL:  branch(gpr[rs] == gpr[rt], b_target, 1); break;
        case OP_BNEL:  branch(gpr[rs] != gpr[rt], b_target, 1); break;
        // BLEZL/BGTZL only have the rs operand; an encoding with rt != 0 is
        // reserved and raises RI, like the RTL (Decode.v OP_BLEZL/OP_BGTZL).
        case OP_BLEZL:
            if (rt == 0) branch((s32)gpr[rs] <= 0, b_target, 1);
            else         exception(RESVINST);
            break;
        case OP_BGTZL:
            if (rt == 0) branch((s32)gpr[rs] >  0, b_target, 1);
            else         exception(RESVINST);
            break;
        case OP_SPECIAL2:
            switch (funct) {
                // MUL writes the low 32 bits of the signed product to rd and
                // leaves HI/LO untouched (MIPS32 SPECIAL2; Decode.v SP2_MUL).
                case SP2_MUL:   writeGPR(rd, (u32)((s64)(s32)gpr[rs] * (s64)(s32)gpr[rt])); break;
                case SP2_MADD:  setHilo(hilo() + (u64)((s64)(s32)gpr[rs] * (s64)(s32)gpr[rt])); break;
                case SP2_MADDU: setHilo(hilo() + (u64)gpr[rs] * (u64)gpr[rt]); break;
                case SP2_MSUB:  setHilo(hilo() - (u64)((s64)(s32)gpr[rs] * (s64)(s32)gpr[rt])); break;
                case SP2_MSUBU: setHilo(hilo() - (u64)gpr[rs] * (u64)gpr[rt]); break;
                case SP2_CLO:   writeGPR(rd, clz(~gpr[rs])); break;
                case SP2_CLZ:   writeGPR(rd, clz(gpr[rs])); break;
                default: exception(RESVINST); break;
            }
            break;
        
        case OP_LB:    load(rt, vaddr, 1, 1); break;
        case OP_LBU:   load(rt, vaddr, 1, 0); break;
        case OP_LH:    load(rt, vaddr, 2, 1); break;
        case OP_LHU:   load(rt, vaddr, 2, 0); break;
        case OP_LW:    load(rt, vaddr, 4, 0); break;
        case OP_LWL:   load_ual(rt, vaddr, 1); break;
        case OP_LWR:   load_ual(rt, vaddr, 0); break;
        case OP_SB:    store(rt, vaddr, 1); break;
        case OP_SH:    store(rt, vaddr, 2); break;
        case OP_SW:    store(rt, vaddr, 4); break;
        case OP_SWL:   store_ual(rt, vaddr, 1); break;
        case OP_SWR:   store_ual(rt, vaddr, 0); break;
        case OP_CACHE: break;
        case OP_LL:    load_link(rt, vaddr); break;
        case OP_PREF:  break;
        case OP_SC:    store_cond(rt, vaddr); break;
        // Same rule as OP_COP1 above: unenabled coprocessor accesses raise
        // CpU with Cause.CE naming the coprocessor (Vol III 6.2.22 lists
        // LWC1/LDC1/SWC1/SDC1 alongside COP1; CP2 gets CE = 2, CP3 CE = 3).
        case OP_LWC1:
        case OP_LDC1:
        case OP_SWC1:
        case OP_SDC1:  exception(CP_UNUSABLE, 1); break;
        case OP_LWC2:
        case OP_LDC2:
        case OP_SWC2:
        case OP_SDC2:  exception(CP_UNUSABLE, 2); break;
        default: exception(RESVINST); break;
    };
}

void MIPS32_Core::exception(Exception e, u32 info, bool isStore)
{

    // info:
    // CpU: coprocessor id
    // AdE/TLBR/TLBI/TLBM: BadVAddr

#if CEMU_TRACE
    fprintf(stderr, "[EXC %d] pc=%08x delay=%d badv=%08x status=%08x cause=%08x\n",
            (int)e, pc, (int)inDelaySlot, cp0.read(CP0_BADVADDR),
            cp0.read(CP0_STATUS), cp0.read(CP0_CAUSE));
#endif

    if (exception_flag && e > exception_type)
        return;
    exception_flag = true;
    branch_taken = false; // ! Observer if it is needed
    exception_type = e;
    
    u32 exc_pc;
    bool bev = cp0.Status_BEV();
    bool iv  = cp0.Cause_IV();
    bool exl = cp0.Status_EXL();
    u32 exc_vector_base = bev ? 0xbfc00200 : 0x80000000;
    
    switch (e) {
        case INTERRUPT:
            if (iv)  exc_pc = exc_vector_base + 0x200;
            else    exc_pc = exc_vector_base + 0x180;
            break;
        
        case I_TLBR:
        case D_TLBR:
            // TLB Refill goes to the refill vector (base) when not nested,
            // otherwise it degrades to the general exception vector (+0x180),
            // matching MangoMIPS32 Control.v {bev, exl} selection.
            if (exl) exc_pc = exc_vector_base + 0x180;
            else     exc_pc = exc_vector_base;
            break;
        
        case ERET:
            exc_pc = cp0.Status_ERL() ? cp0.read(CP0_ERROREPC) : cp0.read(CP0_EPC);
            llbit = 0;
            break;

        default:
            exc_pc = exc_vector_base + 0x180;
    };
    nextpc = exc_pc;

    cp0.exception(e, pc, info, isStore, inDelaySlot);
}

void MIPS32_Core::branch(bool cond, u32 addr, bool likely, u32 link_reg)
{
    branch_flag = true; 
    if (cond) {
        branch_taken = true;
        branch_addr = addr;
    }
    else if (likely) {
        // Nullified delay slot: execution resumes at pc + 8, so the
        // instruction fetched from there is NOT in a delay slot.  Clearing
        // branch_flag keeps renewpc() from marking it as one - otherwise an
        // exception there reports EPC = pc - 4 with Cause.BD = 1, off by one
        // instruction.  MangoMIPS32 gets the same effect from Decode.v's
        // clrslot, which turns the nullified slot into a nop in IF/ID
        // (Reg_IF_ID.v) so the next instruction is no longer marked as a slot.
        nextpc = pc + 8;
        branch_flag = false;
    }
    writeGPR(link_reg, pc + 8);
}

void MIPS32_Core::renewpc()
{
    pc = nextpc;
    
    inDelaySlot = branch_flag;
    branch_flag = false;
    
    if (branch_taken) {
        nextpc = branch_addr;
        branch_taken = false;
    }
    else {
        nextpc = pc + 4;
    }
}

void MIPS32_Core::writeGPR(u32 regaddr, u32 data)
{
    if (exception_flag) return;
    if (regaddr == 0) return;
    gpr[regaddr] = data;
    if (tracer != NULL)
        tracer->trace(pc, regaddr, data);
}

bool MIPS32_Core::addrTranslate(u32 vaddr, bool isStore, bool isFetch, u32 &paddr)
{
    // Segment rules follow MangoMIPS32 MMU.v (TLB-based mode) and MIPS32
    // Vol III §4.7 ("Address Translation for the kuseg Segment when
    // StatusERL = 1": kuseg is unmapped when ERL=1, mapped when ERL=0):
    //   kseg0 / kseg1 : unmapped, PA = VA & 0x1FFFFFFF
    //   everything else (kuseg / kseg2 / kseg3): with Status.ERL=1 they are
    //   identity-mapped the same way; otherwise translated through the TLB.
    u32 seg = vaddr >> 29;
    if (seg == 4 || seg == 5 || cp0.Status_ERL()) {
        paddr = vaddr & 0x1FFFFFFF;
        return true;
    }

    // TLB-mapped access (callers already checked alignment & user mode
    // segment permission, so permission faults surface as TLB exceptions).
    u32 pfn;
    bool vld, drt;
    u32 cat, eob;
    if (!cp0.tlbTranslate(vaddr, pfn, vld, drt, cat, eob)) {
        exception(isFetch ? I_TLBR : D_TLBR, vaddr, isStore);
        return false;
    }
    if (!vld) {
        exception(isFetch ? I_TLBI : D_TLBI, vaddr, isStore);
        return false;
    }
    if (isStore && !drt) {
        exception(D_TLBM, vaddr, true);
        return false;
    }
    paddr = (pfn << 12) | (vaddr & ((1u << eob) - 1));
    return true;
}

void MIPS32_Core::load(u32 reg, u32 vaddr, u32 size, bool isSigned)
{
    if (!aligned(vaddr, size) || user_exc(vaddr)) {
        exception(D_ADE, vaddr, 0);
        return;
    }
    u32 paddr;
    if (!addrTranslate(vaddr, false, false, paddr))
        return;
    u32 temp = memory->read(paddr, size);
    u32 res;
    if (size == 1 && isSigned) 
        res = (s32)(s8)temp;
    else if (size == 2 && isSigned)
        res = (s32)(s16)temp;
    else
        res = temp;
    writeGPR(reg, res);
}

void MIPS32_Core::store(u32 reg, u32 vaddr, u32 size)
{
    if (!aligned(vaddr, size) || user_exc(vaddr)) {
        exception(D_ADE, vaddr, 1);
        return;
    }

    u32 paddr;
    if (!addrTranslate(vaddr, true, false, paddr))
        return;
    memory->write(gpr[reg], paddr, size);
}

void MIPS32_Core::load_ual(u32 reg, u32 vaddr, bool left)
{
    if (user_exc(vaddr)) {
        exception(D_ADE, vaddr, 0);
        return;
    }
    u32 paddr;
    if (!addrTranslate(vaddr, false, false, paddr))
        return;
    u32 memdata = memory->read(paddr, 4);
    u32 regdata = gpr[reg];
    u32 res;
    if (left) { // LWL
        switch (vaddr & 0x3) {
            case 0: res = bitConcat(memdata, 24, regdata); break;
            case 1: res = bitConcat(memdata, 16, regdata); break;
            case 2: res = bitConcat(memdata,  8, regdata); break;
            case 3: res = memdata;
        }
    }
    else { // LWR
        switch (vaddr & 0x3) {
            case 0: res = memdata; break;
            case 1: res = bitConcat(regdata >> 24, 24, memdata >>  8); break;
            case 2: res = bitConcat(regdata >> 16, 16, memdata >> 16); break;
            case 3: res = bitConcat(regdata >>  8,  8, memdata >> 24);
        }
    }
    writeGPR(reg, res);
}

void MIPS32_Core::store_ual(u32 reg, u32 vaddr, bool left)
{
    if (user_exc(vaddr)) {
        exception(D_ADE, vaddr, 1);
        return;
    }
    u32 paddr;
    if (!addrTranslate(vaddr, true, false, paddr))
        return;
    u32 regdata = gpr[reg];
    if (left) {
        switch (vaddr & 0x3) {
            case 0: memory->write(regdata >> 24, paddr, 4, 0x000000FF); break;
            case 1: memory->write(regdata >> 16, paddr, 4, 0x0000FFFF); break;
            case 2: memory->write(regdata >>  8, paddr, 4, 0x00FFFFFF); break;
            case 3: memory->write(regdata, paddr, 4);
        }
    }
    else {
        switch (vaddr & 0x3) {
            case 0: memory->write(regdata, paddr, 4); break;
            case 1: memory->write(regdata <<  8, paddr, 4, 0xFFFFFF00); break;
            case 2: memory->write(regdata << 16, paddr, 4, 0xFFFF0000); break;
            case 3: memory->write(regdata << 24, paddr, 4, 0xFF000000);
        }
    }
}

void MIPS32_Core::load_link(u32 reg, u32 vaddr)
{
    if (!aligned(vaddr, 4) || user_exc(vaddr)) {
        exception(D_ADE, vaddr, 0);
        return;
    }
    u32 paddr;
    if (!addrTranslate(vaddr, false, false, paddr))
        return;
    u32 res = memory->read(paddr, 4);
    writeGPR(reg, res);
    llbit = 1;
}

void MIPS32_Core::store_cond(u32 reg, u32 vaddr)
{
    if (!aligned(vaddr, 4) || user_exc(vaddr)) {
        exception(D_ADE, vaddr, 1);
        return;
    }
    // MIPS32 Vol II-A, SC: the alignment check and AddressTranslation are
    // unconditional; only the store itself is gated by LLbit.  MangoMIPS32
    // ALU_EX.v gates both on LLbit (MangoMIPS32/KnownBugs.md BUG-002) - we
    // follow the spec here on purpose, so do not "fix" it to match the RTL.
    u32 paddr;
    if (!addrTranslate(vaddr, true, false, paddr))
        return;
    if (llbit)
        memory->write(gpr[reg], paddr, 4);
    writeGPR(reg, (u32)llbit);
}

void MIPS32_Core::divide(u32 rs, u32 rt, bool isSigned)
{
    // A zero divisor produces HI = LO = 0 and raises nothing: MangoMIPS32
    // Divider.v diverts to its DivByZero state, whose all-zero dividend is what
    // ends up in the 64-bit result.  Without this guard the host's idiv traps
    // and takes the whole emulator down with SIGFPE - reachable from any guest
    // code, since the MIPS32 spec leaves the result UNPREDICTABLE rather than
    // undefined.
    if (gpr[rt] == 0) {
        hi = 0;
        lo = 0;
        return;
    }
    if (isSigned) {
        // Divide sign-extended operands in 64 bits: -2^31 / -1 is the single
        // case whose 32-bit result overflows, and it is the other input that
        // traps the host.  Divider.v divides magnitudes and negates the two
        // results, which agrees with this.
        s64 a = (s32)gpr[rs], b = (s32)gpr[rt];
        lo = (u32)(a / b);
        hi = (u32)(a % b);
    }
    else {
        lo = gpr[rs] / gpr[rt];
        hi = gpr[rs] % gpr[rt];
    }
}

void MIPS32_Core::add_CheckOv(s32 a, s32 b, u32 rd)
{
    // Widen to 64 bits: the range check is exactly ALU_EX.v's `ALU_ADD` rule
    // (`(as & bs & ~rs) | (~as & ~bs & rs)`), and the sum itself cannot
    // overflow.  The old `s32 c = a + b;` was undefined behaviour as soon as
    // it overflowed - i.e. in the very case this function exists to catch.
    s64 res = (s64)a + (s64)b;
    if (res < INT32_MIN || res > INT32_MAX)
        exception(INTOVERFLOW);
    else
        writeGPR(rd, (u32)res);
}

void MIPS32_Core::sub_CheckOv(s32 a, s32 b, u32 rd)
{
    // Same for ALU_EX.v's `ALU_SUB`.  Subtracting directly also removes the
    // old `add_CheckOv(rs, -(s32)rt, rd)` call form, whose negation was UB
    // for rt = 0x80000000 and made SUB report the opposite of the RTL.
    s64 res = (s64)a - (s64)b;
    if (res < INT32_MIN || res > INT32_MAX)
        exception(INTOVERFLOW);
    else
        writeGPR(rd, (u32)res);
}

u32 MIPS32_Core::clz(u32 a)
{
    for (u32 i = 0; i < 32; ++i) {
        if (a & 0x80000000)
            return i;
        a <<= 1;
    }
    return 32;
}

bool MIPS32_Core::usermode()
{
    if (cp0.Status_ERL()) return false;
    if (cp0.Status_EXL()) return false;
    if (!cp0.Status_UM()) return false;
    return true;
}

bool MIPS32_Core::user_exc(u32 addr)
{
    // Same as usermode() && addr[31], but checks UM first: kernel-mode accesses
    // (the vast majority) then short-circuit on a single field.
    return cp0.Status_UM() && (addr & 0x80000000u)
           && !cp0.Status_ERL() && !cp0.Status_EXL();
}

bool MIPS32_Core::aligned(u32 a, u32 align)
{
    u32 mask = align - 1;
    return (a & mask) == 0;
}
