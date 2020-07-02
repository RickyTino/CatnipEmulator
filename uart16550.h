#ifndef __UART16550_H__
#define __UART16550_H__

#include "common_defs.h"
#include "axi.h"
#include "bit_utils.h"

#define UART16550_RBR 0x1000
#define UART16550_THR 0x1000
#define UART16550_IER 0x1004
#define UART16550_IIR 0x1008
#define UART16550_FCR 0x1008
#define UART16550_LCR 0x100C
#define UART16550_MCR 0x1010
#define UART16550_LSR 0x1014
#define UART16550_MSR 0x1018
#define UART16550_SCR 0x101C
#define UART16550_DLL 0x1000
#define UART16550_DLM 0x1004

class UART16550 : public AXI32_Slave {
private:

public:
    UART16550();
    u32 read(u32 addr, u32 size);
    void write(u32 data, u32 addr, u32 size, u32 mask = 0xFFFFFFFF);
};

#endif
