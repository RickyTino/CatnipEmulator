#ifndef __MIPS32_INSTS_H__
#define __MIPS32_INSTS_H__

// Instruction decode
// Opcode
#define         OP_SPECIAL          0x00
#define         OP_REGIMM           0x01
#define         OP_J                0x02
#define         OP_JAL              0x03
#define         OP_BEQ              0x04
#define         OP_BNE              0x05
#define         OP_BLEZ             0x06
#define         OP_BGTZ             0x07
#define         OP_ADDI             0x08
#define         OP_ADDIU            0x09
#define         OP_SLTI             0x0A
#define         OP_SLTIU            0x0B
#define         OP_ANDI             0x0C
#define         OP_ORI              0x0D
#define         OP_XORI             0x0E
#define         OP_LUI              0x0F
#define         OP_COP0             0x10
#define         OP_COP1             0x11
#define         OP_COP2             0x12
#define         OP_COP3             0x13
#define         OP_BEQL             0x14
#define         OP_BNEL             0x15
#define         OP_BLEZL            0x16
#define         OP_BGTZL            0x17
#define         OP_SPECIAL2         0x1C
#define         OP_LB               0x20
#define         OP_LH               0x21
#define         OP_LWL              0x22
#define         OP_LW               0x23
#define         OP_LBU              0x24
#define         OP_LHU              0x25
#define         OP_LWR              0x26
#define         OP_SB               0x28
#define         OP_SH               0x29
#define         OP_SWL              0x2A
#define         OP_SW               0x2B
#define         OP_SWR              0x2E
#define         OP_CACHE            0x2F
#define         OP_LL               0x30
#define         OP_LWC1             0x31
#define         OP_LWC2             0x32
#define         OP_PREF             0x33
#define         OP_LDC1             0x35
#define         OP_LDC2             0x36
#define         OP_SC               0x38
#define         OP_SWC1             0x39
#define         OP_SWC2             0x3A
#define         OP_SDC1             0x3D
#define         OP_SDC2             0x3E

// Function : Opcode = Special
#define         SP_SLL              0x00
#define         SP_MOVCI            0x01
#define         SP_SRL              0x02
#define         SP_SRA              0x03
#define         SP_SLLV             0x04
#define         SP_SRLV             0x06
#define         SP_SRAV             0x07
#define         SP_JR               0x08
#define         SP_JALR             0x09
#define         SP_MOVZ             0x0A
#define         SP_MOVN             0x0B
#define         SP_SYSCALL          0x0C
#define         SP_BREAK            0x0D
#define         SP_SYNC             0x0F
#define         SP_MFHI             0x10
#define         SP_MTHI             0x11
#define         SP_MFLO             0x12
#define         SP_MTLO             0x13
#define         SP_MULT             0x18
#define         SP_MULTU            0x19
#define         SP_DIV              0x1A
#define         SP_DIVU             0x1B
#define         SP_ADD              0x20
#define         SP_ADDU             0x21
#define         SP_SUB              0x22
#define         SP_SUBU             0x23
#define         SP_AND              0x24
#define         SP_OR               0x25
#define         SP_XOR              0x26
#define         SP_NOR              0x27
#define         SP_SLT              0x2A
#define         SP_SLTU             0x2B
#define         SP_TGE              0x30
#define         SP_TGEU             0x31
#define         SP_TLT              0x32
#define         SP_TLTU             0x33
#define         SP_TEQ              0x34
#define         SP_TNE              0x36

// Rt : Opcode = RegImm
#define         RGI_BLTZ             0x00
#define         RGI_BGEZ             0x01
#define         RGI_BLTZL            0x02
#define         RGI_BGEZL            0x03
#define         RGI_TGEI             0x08
#define         RGI_TGEIU            0x09
#define         RGI_TLTI             0x0A
#define         RGI_TLTIU            0x0B
#define         RGI_TEQI             0x0C
#define         RGI_TNEI             0x0D
#define         RGI_BLTZAL           0x10
#define         RGI_BGEZAL           0x11
#define         RGI_BLTZALL          0x12
#define         RGI_BGEZALL          0x13

// Function : Opcode = Special2
#define         SP2_MADD            0x00
#define         SP2_MADDU           0x01
#define         SP2_MUL             0x02
#define         SP2_MSUB            0x04
#define         SP2_MSUBU           0x05
#define         SP2_CLZ             0x20
#define         SP2_CLO             0x21

// Rs : Opcode = COP0
#define         C0_MFC0             0x00
#define         C0_MTC0             0x04
#define         C0_CO               0x10

// Function : Opcode = COP0 and Rs = CO
#define         C0F_TLBR            0x01
#define         C0F_TLBWI           0x02
#define         C0F_TLBWR           0x06
#define         C0F_TLBP            0x08
#define         C0F_ERET            0x18
#define         C0F_WAIT            0x20

// CP0 Regs
#define         CP0_REG(x,y)        ((x << 3) | y)

#define         CP0_INDEX               CP0_REG( 0, 0)
#define         CP0_RANDOM              CP0_REG( 1, 0)
#define         CP0_ENTRYLO0            CP0_REG( 2, 0)
#define         CP0_ENTRYLO1            CP0_REG( 3, 0)
#define         CP0_CONTEXT             CP0_REG( 4, 0)
#define         CP0_PAGEMASK            CP0_REG( 5, 0)
#define         CP0_WIRED               CP0_REG( 6, 0)
#define         CP0_BADVADDR            CP0_REG( 8, 0)
#define         CP0_COUNT               CP0_REG( 9, 0)
#define         CP0_ENTRYHI             CP0_REG(10, 0)
#define         CP0_COMPARE             CP0_REG(11, 0)
#define         CP0_STATUS              CP0_REG(12, 0)
#define         CP0_CAUSE               CP0_REG(13, 0)
#define         CP0_EPC                 CP0_REG(14, 0)
#define         CP0_PRID                CP0_REG(15, 0)
#define         CP0_EBASE               CP0_REG(15, 1)
#define         CP0_CONFIG              CP0_REG(16, 0)
#define         CP0_CONFIG1             CP0_REG(16, 1)
#define         CP0_TAGLO               CP0_REG(28, 0)
#define         CP0_TAGHI               CP0_REG(29, 0)
#define         CP0_ERROREPC            CP0_REG(30, 0)

// Exceptions
#define         EXCCODE_INT         0x00
#define         EXCCODE_MOD         0x01
#define         EXCCODE_TLBL        0x02
#define         EXCCODE_TLBS        0x03
#define         EXCCODE_ADEL        0x04
#define         EXCCODE_ADES        0x05
#define         EXCCODE_IBE         0x06
#define         EXCCODE_DBE         0x07
#define         EXCCODE_SYS         0x08
#define         EXCCODE_BP          0x09
#define         EXCCODE_RI          0x0A
#define         EXCCODE_CPU         0x0B
#define         EXCCODE_OV          0x0C
#define         EXCCODE_TR          0x0D
#define         EXCCODE_FPE         0x0F
// typedef enum {
//     INTERRUPT, 
//     I_ADEL,
//     I_TLBR,
//     I_TLBI, 
//     CP_UNUSABLE,
//     RESVINST,
//     INTOVERFLO,
//     TRAP,
//     SYSCALL,
//     BREAKPOINT,
//     D_ADEL,
//     D_ADES,
//     D_TLBRL,
//     D_TLBRS,
//     D_TLBIL,
//     D_TLBIS,
//     D_TLBM,
//     ERET
// } Exception;

typedef enum {
    INTERRUPT, 
    I_ADE,
    I_TLBR,
    I_TLBI, 
    CP_UNUSABLE,
    RESVINST,
    INTOVERFLOW,
    TRAP,
    SYSCALL,
    BREAKPOINT,
    D_ADE,
    D_TLBR,
    D_TLBI,
    D_TLBM,
    ERET
} Exception;

// Other types
typedef enum { INST,  DATA } IorD;
typedef enum { STORE, LOAD } SorL;


#endif
