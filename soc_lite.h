#ifndef __SOC_LITE_H__
#define __SOC_LITE_H__

#include "common_defs.h"
#include "bit_utils.h"
#include "mips32_core.h"
#include "axi.h"

#define OS_IS_WINDOWS

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


class SoCLite_Confreg : public AXI32_Slave {
private:
    bool uart_end;

private:
    // u8 readb(u32 addr);
    // void writeb(u8 data, u32 addr);
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

    SoCLite_Confreg();
    void cycle();
    void write_uart(u32 data);

    // u32 read(u32 addr, u32 size);
    // void write(u32 data, u32 addr, u32 size, u32 mask = 0xFFFFFFFF);

};

// class SoCLite_RAM : public AXI32_RAM {
// public:
//     SoCLite_RAM();
// };

// class SoCLite_CPU : public MIPS32_Core {

// }

class SoCLite {
private:
    AXI32_Interconnect bus;
    MIPS32_Tracer tracer;
    MIPS32_Core cpu;
    AXI32_RAM sram;
    SoCLite_Confreg confreg;

public:
    SoCLite();
    void run_func();
    void run_perf();
};


#endif
