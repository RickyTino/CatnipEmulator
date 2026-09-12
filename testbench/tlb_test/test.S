#******************** CatnipEmulator *******************
# Filename:   tlb_test/test.S
# Origin:     MangoMIPS32 Testbench/TLB_Test/test.S (RickyTino), v1.0.1
# Adapted:    register base fixes, fail-on-unexpected-exception handler,
#             0xFF-to-virtual-UART completion signal for the Catnip host.
#
# Register Usage:
# a0: base address of kseg0 (0x80000000)
# a1: base address of kseg1 (0xA0000000)
# s0-s3: consts. s0 = 0x01010101, s1 = 0x02020202, and so on.
# t8: number of exceptions taken
# v0: syscall parameter - address to switch to
# v1: syscall parameter - mask of the switching address

.global __start
.set noreorder
.text
.org 0x0
__start:
    # initialize test environment
    la      $sp, 0xa000a000     # stack pointer: 0xA000A000(p0xA000)
    li      $a0, 0x80000000
    li      $a1, 0xA0000000
    li      $s0, 0x01010101
    li      $s1, 0x02020202
    li      $s2, 0x03030303
    li      $s3, 0x04040404

    # Test 0: Mapping when ERL=1
    li      $t0, 0x1314
    sw      $t0, 0x1000($a1)    # save: 0x1314 to v0xA0001000(p0x1000)
    lw      $t1, 0x1000($zero)  # load: v0x00001000(p0x1000), ERL identity map
    li      $t2, 0x0040FF01
    mtc0    $t2, $12            # Status = 0x0040FF01
    nop
    tne     $t0, $t1            # Trap if unequal
    li      $t3, 0x00000003
    mtc0    $t3, $16            # Config.K0 = 3 (Cacheable)
    jal     TLB_test            # branch and link to test
    nop
    # test done: signal host via 0xFF on the virtual UART
    lui     $t0, 0xbfaf
    li      $t1, 0xff
    sw      $t1, 0xfff0($t0)    # 0xbfaf_fff0: VIRTUAL_UART
1:  b       1b
    nop

.org 0x200                      # TLB Refill Entrance (BEV=1)
    b       excp
    nop

.org 0x380
excp:
    addi    $t8, $t8, 1
    mfc0    $k0, $13
    andi    $k0, $k0, 0x7C
    srl     $k0, $k0, 2         # k0 = cp0.cause.exccode

    # Exceptions the test deliberately triggers and swallows:
    #   1 = Mod, 2 = TLBL, 3 = TLBS, 8 = Syscall.
    li      $k1, 1
    beq     $k1, $k0, excp_skip
    nop
    li      $k1, 2
    beq     $k1, $k0, excp_skip
    nop
    li      $k1, 3
    beq     $k1, $k0, excp_skip
    nop
    li      $k1, 8
    beq     $k1, $k0, excp_syscall
    nop
    # Anything else means the test failed: hang forever.
    b       excp_fail
    nop

excp_syscall:
    and     $v0, $v0, $v1       # v0 = v0 & v1(mask)
    nor     $v1, $v1, $zero     # v1 = ~v1
    mfc0    $k0, $14            # k0 = EPC
    and     $k0, $k0, $v1       # k0 = k0 & ~v1(mask)
    or      $k0, $k0, $v0       # k0 = v0 | k0
    addi    $k0, 4
    mtc0    $k0, $14            # EPC = k0
    nop
    eret

excp_skip:                      # swallow a TLB exception, resume
    mfc0    $k0, $14
    addi    $k0, 4
    mtc0    $k0, $14            # EPC = EPC + 4
    nop
    eret

excp_fail:                      # unexpected exception -> FAIL (hang)
1:  b       1b
    nop

.org 0x700
TLB_test:
    li      $t0, 0x10
    mtc0    $t0, $6             # Wired = 16

    # Test1: Basic translation & tlb instructions (ASID Match)
    li      $t1, 0x0003
    mtc0    $t1, $10            # EntryHi: VPN2=0x00000/2, ASID=0x03
    li      $t2, 0x0046
    mtc0    $t2, $2             # EntryLo0: PFN=0x00001, DV
    li      $t3, 0x0006
    mtc0    $t3, $3             # EntryLo1: PFN=0x00000, DV
    tlbwr
    li      $t0, 0
    mtc0    $t0, $0             # Index=0x00
    li      $t1, 0x2005
    mtc0    $t1, $10            # EntryHi: VPN2=0x00001/2, ASID=0x05
    li      $t2, 0x0007
    mtc0    $t2, $2             # EntryLo0: PFN=0x00000, DVG
    li      $t3, 0x0047
    mtc0    $t3, $3             # EntryLo1: PFN=0x00001, DVG
    tlbwi

    li      $t1, 0x0005
    mtc0    $t1, $10            # EntryHi: VPN2=0x00000/2, ASID=0x05
    tlbp                        # Probe
    mfc0    $t0, $0             # t0 = Index
    tgei    $t0, 0              # Trap if match (should be a miss)

    li      $t1, 0x0003
    mtc0    $t1, $10            # EntryHi: VPN2=0x00000/2, ASID=0x03
    tlbp
    mfc0    $t2, $0
    tlti    $t2, 0              # Trap if unmatch

    sw      $s0, 0x0000         # save s0 to v0x00000000(p0x1000)
    sw      $s1, 0x1004($a1)    # save s1 to v0xA0001004(p0x1004)
    sync
    lw      $t3, 0x0004         # load from v0x00000004(p0x1004)
    tne     $t3, $s1            # trap if unequal
    lw      $t4, 0x1000($a1)    # load from v0xA0001000(p0x1000)
    tne     $t4, $s0            # trap if unequal
    lw      $t5, 0x1380         # load from v0x00001380(p0x1380)
    li      $t6, 0x23180001     # <- the first instruction of exception handler
    tne     $t5, $t6            # trap if unequal

    # Test1+: execute code in a TLB-translated virtual page (0x00002000)
    li      $v0, 0x00002000
    li      $v1, 0xFFFFE000
    syscall
    nop                         # the following instruction is in page 0x2000

    # Test2: field G
    li      $t2, 0x2000
    mtc0    $t2, $10            # EntryHi: VPN2=0x00001/2, ASID=0x00
    tlbp                        # probe for the global page
    nop
    mfc0    $t3, $0
    tlti    $t3, 0              # Trap if unmatch

    lw      $t0, 0x2000         # load from v0x00002000(p0x0000)
    lw      $t1, 0x0000($a1)    # load the very first instruction via kseg1
    tne     $t0, $t1            # trap if unequal
    lw      $t2, 0x3000         # load from v0x00003000(p0x1000)
    tne     $t2, $s0            # trap if unequal
    lw      $t3, 0x3004         # load from v0x00003004(p0x1004)
    tne     $t3, $s1            # trap if unequal
    sw      $s2, 0x3008         # save s2 to v0x00003008(p0x1008)

    # Test3: 16KB big page (mask)
    li      $t0, 1
    mtc0    $t0, $0             # Index=0x01
    li      $t1, 0x00010000
    mtc0    $t1, $10            # EntryHi: VPN2=0x00008/2, ASID=0x00
    li      $t2, 0x0006
    mtc0    $t2, $2             # EntryLo0: PFN=0x00000, DV
    li      $t3, 0x0006
    mtc0    $t3, $3             # EntryLo1: PFN=0x00000, DV
    li      $t4, 0x6000
    mtc0    $t4, $5             # PageMask: Mask=0x0003 (16KB)
    tlbwi

    lui     $t0, 0x0001
    lw      $t1, 0x1000($t0)    # load from v0x00011000(p0x1000)
    tne     $t1, $s0
    lw      $t2, 0x1004($t0)    # load from v0x00011004(p0x1004)
    tne     $t2, $s1
    lw      $t3, 0x1008($t0)    # load from v0x00011008(p0x1008)
    tne     $t3, $s2

    sw      $s3, 0x3000($a1)    # save s3 to v0xA0003000(p0x3000)
    lw      $t6, 0x3000($t0)    # load from v0x00013000(p0x3000)
    tne     $t6, $s3
    lw      $t7, 0x7000($t0)    # load from v0x00017000(p0x3000)
    tne     $t7, $s3

    # Test 4: Cacheability attribute test
    li      $t1, 0x00006000
    mtc0    $t1, $10            # EntryHi: VPN2=0x00003/2, ASID=0x00
    li      $t2, 0x001F
    mtc0    $t2, $2             # EntryLo0: PFN=0x00000, CDVG
    li      $t3, 0x005F
    mtc0    $t3, $3             # EntryLo1: PFN=0x00001, CDVG
    li      $t4, 0x00
    mtc0    $t4, $5             # PageMask: Mask=0x0000
    tlbwr

    lw      $t0, 0x600C         # load from v0x0000600C(p0x000C)
    lw      $t1, 0x000C($a0)    # load from v0x8000000C(p0x000C)
    lw      $t2, 0x7004         # load from v0x00007004(p0x1004)
    tne     $t2, $s1

    # Test 5: TLB exceptions
    lw      $t0, 0x4000         # load: TLBRefill-TLBL
    sw      $t0, 0x4000         # save: TLBRefill-TLBS

    li      $t0, 2
    mtc0    $t0, $0             # Index=0x02
    li      $t1, 0x4000
    mtc0    $t1, $10            # EntryHi: VPN2=0x00002/2, ASID=0x00
    li      $t2, 0x0005
    mtc0    $t2, $2             # EntryLo0: PFN=0x00000, DG
    li      $t3, 0x0043
    mtc0    $t3, $3             # EntryLo1: PFN=0x00001, VG
    tlbwi
    lw      $t0, 0x4000         # load: TLBInvalid-TLBL
    sw      $t0, 0x4000         # save: TLBInvalid-TLBS
    lw      $t1, 0x5000         # load from v0x00005000(p0x1000)
    tne     $t1, $s0
    sw      $t1, 0x5000         # save: TLBModified-TLBS
    sync

    # Test 6: Getting back to kseg1 address space
    li      $v0, 0xBFC00000
    li      $v1, 0xFFFFE000
    syscall
    nop

    # Test end, return
    jr      $ra
    nop
