# DIV with a zero divisor: HI and LO must both become 0 and no exception may be
# raised.  MangoMIPS32's Divider.v diverts to its DivByZero state, whose
# all-zero dividend is what ends up in the 64-bit result; the MIPS32 manual
# only says the result is UNPREDICTABLE, so following the RTL is the choice.
#
# This is a regression test in the loud sense: before the guard was added the
# host's idiv trapped and took the whole emulator down with SIGFPE.
#
# HI/LO are poisoned first so that a missing write shows up as 0xDEADBEEF
# rather than as the expected zeroes.
    .set noreorder
    .set nomacro
    .text
    .globl _start
_start:
    lui   $t4, 0xDEAD
    ori   $t4, $t4, 0xBEEF
    mthi  $t4
    mtlo  $t4

    lui   $2, 0x1234
    ori   $2, $2, 0x5678
    .word 0x0040001A                # div $2, $0 (raw: see the Makefile)

    mfhi  $3
    mflo  $4
    sw    $3, 0x100($0)             # result[0] = HI
    sw    $4, 0x104($0)             # result[1] = LO
1:
    beq   $0, $0, 1b
    nop
