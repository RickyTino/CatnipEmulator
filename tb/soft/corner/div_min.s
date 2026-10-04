# -2^31 / -1: the one signed division whose 32-bit quotient overflows, and the
# second input that traps the host's idiv.  Divider.v divides magnitudes and
# negates the results, which gives LO = 0x80000000 and HI = 0.
#
# Like div_zero, HI/LO are poisoned first so a missing write is visible.
    .set noreorder
    .set nomacro
    .text
    .globl _start
_start:
    lui   $t4, 0xDEAD
    ori   $t4, $t4, 0xBEEF
    mthi  $t4
    mtlo  $t4

    lui   $2, 0x8000                # -2^31
    addiu $3, $0, -1
    .word 0x0043001A                # div $2, $3 (raw: see the Makefile)

    mfhi  $4
    mflo  $5
    sw    $4, 0x100($0)             # result[0] = HI
    sw    $5, 0x104($0)             # result[1] = LO
1:
    beq   $0, $0, 1b
    nop
