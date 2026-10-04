# Shared exception vector for the corner tests.  Staged at 0x1FC00380, i.e.
# 0xBFC00380 - the general exception vector while Status.BEV = 1 (the reset
# value).  It saves the two CP0 registers the tests assert on into the result
# block the host reads back, then spins.
#
# Result block (see tb/corner_tests.cpp): result[0] at 0x100, result[1] at
# 0x104, physical addresses that ERL = 1 leaves identity-mapped.
    .set noreorder
    .set nomacro
    .text
    .globl _start
_start:
    mfc0  $k0, $14                  # EPC
    mfc0  $k1, $13                  # Cause
    sw    $k0, 0x100($0)
    sw    $k1, 0x104($0)
1:
    beq   $0, $0, 1b
    nop
