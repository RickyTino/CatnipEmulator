#ifndef __NSCSCC_TESTS_H__
#define __NSCSCC_TESTS_H__

#include "common_defs.h"
#include "bit_utils.h"
#include "mips32_core.h"
#include "axi.h"
#include <time.h>

#ifdef OS_IS_WINDOWS
    #include <windows.h>
#endif

#define SRAM_SIZE           1048576  // 1M
#define SRAM_WIDTH          20       // 1M, 0x100000

// Confregs addr offset
#define CR0_ADDR            0x8000   //32'hbfaf_8000
#define CR1_ADDR            0x8004   //32'hbfaf_8004
#define CR2_ADDR            0x8008   //32'hbfaf_8008
#define CR3_ADDR            0x800c   //32'hbfaf_800c
#define CR4_ADDR            0x8010   //32'hbfaf_8010
#define CR5_ADDR            0x8014   //32'hbfaf_8014
#define CR6_ADDR            0x8018   //32'hbfaf_8018
#define CR7_ADDR            0x801c   //32'hbfaf_801c
#define LED_ADDR            0xf000   //32'hbfaf_f000
#define LED_RG0_ADDR        0xf004   //32'hbfaf_f004
#define LED_RG1_ADDR        0xf008   //32'hbfaf_f008
#define NUM_ADDR            0xf010   //32'hbfaf_f010
#define SWITCH_ADDR         0xf020   //32'hbfaf_f020
#define BTN_KEY_ADDR        0xf024   //32'hbfaf_f024
#define BTN_STEP_ADDR       0xf028   //32'hbfaf_f028
#define SW_INTER_ADDR       0xf02c   //32'hbfaf_f02c
#define TIMER_ADDR          0xe000   //32'hbfaf_e000
#define IO_SIMU_ADDR        0xffec   //32'hbfaf_ffec
#define VIRTUAL_UART_ADDR   0xfff0   //32'hbfaf_fff0
#define SIMU_FLAG_ADDR      0xfff4   //32'hbfaf_fff4
#define OPEN_TRACE_ADDR     0xfff8   //32'hbfaf_fff8
#define NUM_MONITOR_ADDR    0xfffc   //32'hbfaf_fffc

// Test programs, loaded straight from their ELF build output (paths are
// relative to the project root, where ./nscscc_tests runs).
//
// The TLB suite (tb/soft/tlb_func/) reports its result by writing
// LED_RG0/LED_RG1 - 1 = every test point passed, 2 = one failed (see its
// start.S test_end), then spins forever.  run_tlb() watches those registers
// for completion; this program has no UART terminator.
//
// The functional suite (tb/soft/func/) ends the same way: its start.S
// test_end writes the LEDs (1/1 = all points passed, 2/2 = one failed) and
// then falls into test_finish, which spins forever writing 0xFF to the
// virtual UART.  run_func() stops on either signal.
#define NSCSCC_FUNC_ELF     "tb/soft/func/obj/main.elf"
#define NSCSCC_PERF_ELF     "tb/soft/perf_func/obj/allbench/main.elf"
#define NSCSCC_TLB_ELF      "tb/soft/tlb_func/obj/main.elf"

// Reference trace for the functional test; only the func test uses it, so it
// lives next to that program.  (The release ships it under
// func_test_v0.01/cpu132_gettrace/, not in soft/.)
#define NSCSCC_GOLDEN_TRACE "tb/soft/func/golden_trace.txt"

class NscsccConfreg : public AXI32_Slave {
private:
    u32 readw(u32 addr);
    void writew(u32 data ,u32 addr, u32 mask);

public:
    u32 cr[8]; //confreg registers

    u32 led;
    u32 led_rg0, led_rg1;
    u32 digit;
    u32 swt;
    u32 swinter;
    u32 timer;

    u32 iosimu;
    u32 uart;
    u32 simflag;
    bool opentrace;
    u32 monitor;
    bool uart_end;

    NscsccConfreg();
    void cycle();
    void write_uart(u32 data);
};

// NSCSCC contest test harness: the legacy confreg + SRAM machine used by the
// functional / performance / TLB test programs.
class NscsccTests {
private:
    AXI32_Interconnect bus;
    MIPS32_Tracer tracer;
    MIPS32_Core cpu;
    AXI32_RAM sram;
    NscsccConfreg confreg;

    // Load a test program from its ELF build output and point the CPU at the
    // image entry.  Exits non-zero if the image cannot be loaded.
    void loadTestImage(const char *path);

public:
    NscsccTests();
    void run_func();
    void run_perf();
    void run_tlb();
};

#endif
