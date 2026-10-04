# The instruction after a NULLIFIED likely-branch delay slot is not itself in a
# delay slot, so the unaligned load below must report EPC = 0xBFC00008 with
# Cause.BD = 0.
#
# Before the fix the stale delay-slot flag made it report EPC = 0xBFC00004
# (the nullified slot) with Cause.BD = 1, which tells an exception handler to
# re-execute the wrong instruction.
#
# bltzall $0 is never taken ($0 is not negative), so the addiu is the slot
# that gets nullified.
    .set noreorder
    .set nomacro
    .text
    .globl _start
_start:
    bltzall $0, target
    addiu   $3, $0, 0x1111          # nullified slot: must never execute
    lw      $2, 1($0)               # unaligned -> D_AdE
    nop
target:
    beq     $0, $0, target
    nop
