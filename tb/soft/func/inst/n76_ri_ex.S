#include <asm.h>
#include <regdef.h>
#include <inst_test.h>

LEAF(n76_ri_ex_test)
    .set noreorder
    addiu s0, s0, 1
    li    t0, 0x800d0000
    li    s2, 0x07
    sw    s2, 0(t0)
##clear cause.TI, status.EXL
    mtc0  zero, c0_compare
    lui   s7,0x0040
	mtc0  s7, c0_status
    nop
    lui   s7, 0x0007      #add ex, ref return value.
###test inst
 ##1
    la    s4, 1f
/* This word was 0x45df00e0 until 2026-10-05, when it turned out to be a COP1
   encoding: on a machine with no CP1 and Status.CU1 = 0 the architecture
   requires Coprocessor Unusable for COP1 - not Reserved Instruction (Vol III
   6.2.22 lists COP1 together with LWC1/SWC1/LDC1/SDC1, and 6.1 makes CpU
   outrank RI when both apply).  So the old word could not test what this test
   is for.  It is now opcode 0x1E, reserved, as in case ##2 below; the golden
   trace still matches, because the exception path - and hence the recorded PC
   and register stream - is the same.  The original word is in git history:
   "git show HEAD:tb/soft/func/inst/n76_ri_ex.S".  The image copies
   (obj/main.elf, obj/main.bin, obj/inst_ram.coe) are patched to match; do not
   rebuild main.elf from source, since a different toolchain would move the
   whole program and invalidate the golden trace. */
1:  TEST_RI_EX(0x79df00e0)
    bne s2, s7, inst_error
    nop
 ##2
    li    s2, 0x07
    la    s4, 1f
    sw    t0, 4(t0)
    sw    s4, 4(t0) 
1:  TEST_RI_EX(0x7a22db9c)
    sw    s4, 0(t0) 
    lw    t1, 4(t0)
    bne t1, s4, inst_error
    nop
    bne s2, s7, inst_error
    nop
    li    s2, 0x07
    sw    s2, 0(t0)
 ##3
    li    s2, 0x07
    la    s4, 1f
    mthi  t0
    divu  zero, t0, s0
1:  TEST_RI_EX(0x6f76e210)
    mfhi  t1
    beq   t1, t0, inst_error
    nop
    bne s2, s7, inst_error
    nop
 ##4
    li    s2, 0x07
    la    s4, 1f
1:  TEST_RI_EX(0x5a8d78ce)
    divu  zero, s0, t0
    bne s2, s7, inst_error
    nop
 ##5
    li    s2, 0x07
    la    s4, 1f
    mtlo  t0
    multu t0, s0
1:  TEST_RI_EX(0x7bd1ac60)
    mfhi  t1
    beq   t1, t0, inst_error
    nop
    bne s2, s7, inst_error
    nop
 ##6
    li    s2, 0x07
    la    s4, 1f
1:  TEST_RI_EX(0x7e6265cc)
    multu t0, s2
    bne s2, s7, inst_error
    nop
    bne v0, v1, inst_error
    nop
 ##7
    li    s2, 0x07
    la    s4, 1f
    mtc0  s2, c0_epc
1:  TEST_RI_EX(0x9fc55510)
    mtc0 t0, c0_epc
    bne s2, s7, inst_error
    nop
    bne v0, v1, inst_error
    nop
###score ++
    addiu s3, s3, 1
###output (s0<<24)|s3
inst_error:  
    sll t1, s0, 24
    or t0, t1, s3 
    sw t0, 0(s1)
    jr ra
    nop
END(n76_ri_ex_test)
