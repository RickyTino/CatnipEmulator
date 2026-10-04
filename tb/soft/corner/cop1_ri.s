# mfc1 on a core with no CP1: this emulator raises Reserved Instruction
# (Cause.ExcCode = 0x0A, Cause.CE = 0), not Coprocessor Unusable.
#
# That is what the NSCSCC golden trace demands - it sends this encoding to its
# reserved-instruction handler, and reporting CpU instead fails the functional
# test at trace record 86034 - even though Decode.v (which leaves instvalid
# clear for OP_COP1 while raising exc_cpu) plus Exception.v's priority encoder
# would give CpU.  The trace wins; see the comment above OP_COP1 in
# src/cpu/mips32_core.cpp.
    .set noreorder
    .set nomacro
    .text
    .globl _start
_start:
    .word 0x44020800                # mfc1 $2, $1
1:
    beq   $0, $0, 1b
    nop
