# Reads from addresses no slave decodes.  Two properties are asserted:
#   * the value read is 0 and the program carries on (guest-visible behaviour
#     is unchanged - this is not an exception, just an undecoded access), and
#   * the interconnect reports each distinct address once on stderr, up to
#     eight, then says once that further ones are suppressed.
#
# Ten distinct addresses plus two repeats of the first: eight per-address lines
# and one suppression notice, no more.
#
# 0xA8000000 is KSEG1 for physical 0x08000000, past the test machine's 1MB of
# RAM.  (Careful with the mask: 0xA0000000 & 0x1FFFFFFF is 0, i.e. RAM - it
# does not reach an undecoded address at all.)
    .set noreorder
    .set nomacro
    .text
    .globl _start
_start:
    lui   $4, 0xA800                # first address; also the repeat target
    addiu $3, $4, 0
    addiu $5, $0, 10                # ten distinct addresses, 0x1000 apart
1:
    lw    $2, 0($3)
    addiu $3, $3, 0x1000
    addiu $5, $5, -1
    bne   $5, $0, 1b
    nop

    lw    $2, 0($4)                 # repeats of an address already reported
    lw    $2, 0($4)
    sw    $2, 0x100($0)             # result[0] = last value read (must be 0)
2:
    beq   $0, $0, 2b
    nop
