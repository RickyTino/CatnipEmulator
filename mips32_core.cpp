#include "mips32_core.h"

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
    hilo = 0;
    llbit = false;
    branch_flag = false;
    branch_taken = false;
    inDelaySlot = false;
    exception_flag = false;
    cp0.reset();
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
    // pc = nextpc;
    renewpc();
    // nextpc = pc + 4;

    // CP0 cycle
    cp0.cycle();

    // Check interrupt
    if (cp0.hasInterrupt()) 
        exception(INTERRUPT);

    // Inst fetch
    inst = fetch(pc);
    
    // TEMP
//    cout << "0x" << hex << pc << ": ";
//    cout << "0x" << hex << inst << endl;

    // Execute
    execute(inst);
}

bool* MIPS32_Core::get_irq(u32 irq_num)
{
    if(irq_num > 7) return NULL;
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

    u32 b_target = pc + (simme << 2) + 4;
    u32 j_target = (pc & 0xF0000000) | (j_offset << 2);
    u32 vaddr    = (s32)gpr[rs] + simme;

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
                case SP_MULT:    hilo = (s64)(s32)gpr[rs] * (s64)(s32)gpr[rt]; break;
                case SP_MULTU:   hilo = (u64)gpr[rs] * (u64)gpr[rt]; break;
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

        case OP_COP1:  exception(RESVINST); break; // exception(CP_UNUSABLE, 1); break;
        case OP_COP2:  exception(CP_UNUSABLE, 2); break;
        case OP_COP3:  exception(CP_UNUSABLE, 3); break;
        case OP_BEQL:  branch(gpr[rs] == gpr[rt], b_target, 1); break;
        case OP_BNEL:  branch(gpr[rs] != gpr[rt], b_target, 1); break;
        case OP_BLEZL: branch((s32)gpr[rs] <= 0, b_target, 1); break;
        case OP_BGTZL: branch((s32)gpr[rs] >  0, b_target, 1); break;
        case OP_SPECIAL2:
            switch (funct) {
                case SP2_MADD:  hilo += (s64)(s32)gpr[rs] * (s64)(s32)gpr[rt]; break;
                case SP2_MADDU: hilo += (u64)gpr[rs] * (u64)gpr[rt]; break;
                case SP2_MSUB:  hilo -= (s64)(s32)gpr[rs] * (s64)(s32)gpr[rt]; break;
                case SP2_MSUBU: hilo -= (u64)gpr[rs] * (u64)gpr[rt]; break;
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

// void MIPS32_Core::exception(Exception e)
// {

//     if (exception_flag && e > exception_type)
//         return;
//     exception_flag = true;
//     branch_taken = false; // ! Observer if it is needed
//     exception_type = e;
//     cp0.exception(e);

//     u32 exc_pc;
//     bool bev = cp0.Status_BEV();
//     bool iv  = cp0.Cause_IV();
//     bool exl = cp0.Status_EXL();
//     u32 exc_vector_base = bev ? 0xbfc00200 : 0x80000000;
//     switch (e) {
//         case INTERRUPT:
//             if (iv)  exc_pc = exc_vector_base + 0x200;
//             else    exc_pc = exc_vector_base + 0x180;
//             break;
        
//         case I_TLBR:
//         case D_TLBRL:
//         case D_TLBRS:
//             if (exl) exc_pc = exc_vector_base;
//             else    exc_pc = exc_vector_base + 0x180;
        
//         case ERET:
//             exc_pc = cp0.Status_ERL() ? cp0.read(ERROREPC) : cp0.read(EPC);

//         default:
//             exc_pc = exc_vector_base + 0x180;
//     };
//     nextpc = exc_pc;
// }

void MIPS32_Core::exception(Exception e, u32 info, bool isStore)
{

    // info:
    // CpU: coprocessor id
    // AdE/TLBR/TLBI/TLBM: BadVAddr

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
    
//    cout << "Exception: " << e << endl
//         << "PC: " << hex << pc << endl
//         << "Info: " << info << endl
//         << "EPC: " << cp0.read(CP0_EPC) << endl;
//    if(inDelaySlot) cout << "In Delay Slot!" << endl; 
}

// void MIPS32_Core::branch(bool cond, bool link, bool likely, u32 addr)
void MIPS32_Core::branch(bool cond, u32 addr, bool likely, u32 link_reg)
{
    branch_flag = true; 
    if (cond) {
        branch_taken = true;
        branch_addr = addr;
    }
    else if (likely) {
        nextpc = pc + 8;
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

// void MIPS32_Core::multiply(u32 rs, u32 rt, bool isSigned)
// {
//     if (isSigned)
//         hilo = (s64)(s32)gpr[rs] * (s64)(s32)gpr[rt];
//     else
//         hilo = (u64)gpr[rs] * (u64)gpr[rt];
// }

void MIPS32_Core::divide(u32 rs, u32 rt, bool isSigned)
{
    if (isSigned) {
        lo = (s32)gpr[rs] / (s32)gpr[rt];
        hi = (s32)gpr[rs] % (s32)gpr[rt];
    }
    else {
        lo = gpr[rs] / gpr[rt];
        hi = gpr[rs] % gpr[rt];
    }
}

// bool MIPS32_Core::intOverflow(s32 a, s32 b)
// {
//     s32 c = a + b;
//     return (a >= 0 && b >= 0 && c < 0) || (a < 0)&&(b < 0)&&(c >= 0);
// }

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
    return (usermode() && GETBIT(addr, 31));
}

bool MIPS32_Core::aligned(u32 a, u32 align)
{
    u32 mask = align - 1;
    return (a & mask) == 0;
}
