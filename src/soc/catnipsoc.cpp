#include "catnipsoc.h"

static const struct {
    const char *name;
    u32 base;
    u32 len;
} SOC_REGIONS[] = {
    { "memory",  CATNIPSOC_MEM_BASE,  CATNIPSOC_MEM_SIZE  },
    { "uart",    CATNIPSOC_UART_BASE, CATNIPSOC_UART_LEN  },
    { "ethernetlite", CATNIPSOC_ETH_BASE, CATNIPSOC_ETH_LEN },
    { "bootrom", CATNIPSOC_BOOT_BASE, CATNIPSOC_BOOT_SIZE },
};

// Initialization order follows the member declaration order in catnipsoc.h
// (bus, cpu, memory, bootrom, uart, eth).
CatnipSoC::CatnipSoC() :
bus(0, 0),
cpu(&bus),
memory(CATNIPSOC_MEM_BASE, CATNIPSOC_MEM_SIZE, CATNIPSOC_MEM_WIDTH),
bootrom(CATNIPSOC_BOOT_BASE, CATNIPSOC_BOOT_SIZE, CATNIPSOC_BOOT_WIDTH),
uart(CATNIPSOC_UART_BASE, CATNIPSOC_UART_LEN, CATNIPSOC_UART_OFFSET),
eth(CATNIPSOC_ETH_BASE, CATNIPSOC_ETH_LEN)
{
    bus.addSlave(&memory);
    bus.addSlave(&bootrom);
    bus.addSlave(&uart);
    bus.addSlave(&eth);

    // The SoC wires the UART interrupt straight to CPU IP2.
    uart.setIRQLine(cpu.get_irq(CATNIPSOC_UART_IRQ));
}

void CatnipSoC::step()
{
    cpu.cycle();
    uart.tick();
}

const char *CatnipSoC::regionName(u32 addr) const
{
    for (u32 i = 0; i < sizeof(SOC_REGIONS) / sizeof(SOC_REGIONS[0]); ++i) {
        if (addr >= SOC_REGIONS[i].base &&
            addr <  SOC_REGIONS[i].base + SOC_REGIONS[i].len)
            return SOC_REGIONS[i].name;
    }
    return "unmapped";
}
