# Companion to likely_null.s: here the delay slot of a TAKEN likely branch
# really runs and really faults, so the exception must report the branch
# itself - EPC = 0xBFC00000, Cause.BD = 1 - which is the convention that lets
# a handler re-execute the branch.  This is the guard against clearing the
# delay-slot flag too eagerly.
#
# bgezall $0 is always taken ($0 is not negative), so the lw below it is a
# genuine delay slot.
    .set noreorder
    .set nomacro
    .text
    .globl _start
_start:
    bgezall $0, target
    lw      $2, 1($0)               # faults in the delay slot
    nop
target:
    beq     $0, $0, target
    nop
