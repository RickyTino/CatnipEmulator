# mfc1 on a core with no CP1 must raise Coprocessor Unusable - Cause.ExcCode =
# 0x0B with Cause.CE = 1.  Vol III 6.2.22 lists COP1 in one clause together
# with LWC1/SWC1/LDC1/SDC1, and 6.1 gives CpU priority over RI when both apply
# to the same instruction.  The RTL agrees (Decode.v raises exc_cpu, and
# Exception.v orders CpU before RI).
#
# This case used to assert RI: the golden trace's reference core answered RI
# for a COP1 word inside the functional test's reserved-instruction test, and
# the emulator matched the trace.  That word has since been replaced with a
# genuinely reserved encoding (see tb/soft/func/inst/n76_ri_ex.S), so this case
# now pins the spec-conformant answer instead.  Without it, nothing in the test
# suites would notice if this exception code drifted again.
    .set noreorder
    .set nomacro
    .text
    .globl _start
_start:
    .word 0x44020800                # mfc1 $2, $1
1:
    beq   $0, $0, 1b
    nop
