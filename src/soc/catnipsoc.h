#ifndef __CATNIPSOC_H__
#define __CATNIPSOC_H__

#include "common_defs.h"
#include "axi.h"
#include "mips32_core.h"
#include "uart16550.h"

// Minimal CatnipSoC: just CPU + memory + bootrom + uart16550, no golden-trace
// tracer.  The physical map follows CatnipSoC
// (arch/mips/boot/dts/catnipsoc/catnipsoc.dts):
//   0x00000000..0x07FFFFFF  memory   (128MB)
//   0x10400000..0x1040FFFF  uart16550 (DT reg-offset 0x1000)
//   0x1FC00000..0x1FCFFFFF  bootrom  (1MB, MIPS reset vector 0xBFC00000)
// Everything else reads as 0 / ignores writes.
#define CATNIPSOC_MEM_BASE    0x00000000
#define CATNIPSOC_MEM_WIDTH   27                    // 1 << 27 = 128MB
#define CATNIPSOC_MEM_SIZE    (1u << CATNIPSOC_MEM_WIDTH)

#define CATNIPSOC_UART_BASE   0x10400000
#define CATNIPSOC_UART_LEN    0x10000
#define CATNIPSOC_UART_OFFSET 0x1000                // DT reg-offset
#define CATNIPSOC_UART_IRQ    2                     // DT interrupts = <2> -> IP2

#define CATNIPSOC_BOOT_BASE   0x1FC00000
#define CATNIPSOC_BOOT_WIDTH  20                    // 1 << 20 = 1MB
#define CATNIPSOC_BOOT_SIZE   (1u << CATNIPSOC_BOOT_WIDTH)

// MIPS reset vector (KSEG1): matches MIPS32_Core::reset().
#define CATNIPSOC_RESET       0xBFC00000

class CatnipSoC {
private:
    AXI32_Interconnect bus;
    MIPS32_Core cpu;             // no tracer: boots a real image, not a trace
    AXI32_RAM memory;
    AXI32_RAM bootrom;
    UART16550 uart;

public:
    CatnipSoC();

    // One emulated cycle: run the CPU, then poll the console.
    void step();

    AXI32_RAM &getMemory()  { return memory;  }
    AXI32_RAM &getBootrom() { return bootrom; }
    UART16550 &getUart()    { return uart;    }
    MIPS32_Core &getCpu()   { return cpu;     }
    AXI32_Interconnect &getBus() { return bus; }

    // Name of the region decoding a physical address ("memory"/"uart"/"bootrom"
    // /"unmapped"), for image-loader logging.
    const char *regionName(u32 addr) const;
};

#endif
