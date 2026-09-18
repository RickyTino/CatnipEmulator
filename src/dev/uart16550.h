#ifndef __UART16550_H__
#define __UART16550_H__

#include "common_defs.h"
#include "axi.h"

#ifndef OS_IS_WINDOWS
    #include <termios.h>
#endif

// Minimal NS16550A UART modelled as an AXI32_Slave.  Linux reaches it through
// 8250_of ("ns16550a" -> PORT_16550A), so only the subset of behaviour that
// driver and its console rely on is implemented here.
//
// The class itself carries no absolute address.  As with the CatnipSoC device
// tree (arch/mips/boot/dts/catnipsoc/catnipsoc.dts):
//   reg = <0x10400000 0x10000>, reg-offset = <0x1000>, reg-shift = <2>,
//   reg-io-width = <4>
// the instantiation picks the decoded range (base, len) and the IP register
// window offset within it (regOffset, from the DT "reg-offset" property).
// Register index n then lives at base + regOffset + n*4, accessed 32-bit.

// Register index offsets, relative to the start of the register window.  These
// are the standard 16550 offsets; the device tree reg-offset (+ reg-shift) is
// applied on top by the constructor, so they contain no SoC address.
#define UART16550_RBR 0x00
#define UART16550_THR 0x00
#define UART16550_DLL 0x00
#define UART16550_IER 0x04
#define UART16550_DLM 0x04
#define UART16550_IIR 0x08
#define UART16550_FCR 0x08
#define UART16550_LCR 0x0C
#define UART16550_MCR 0x10
#define UART16550_LSR 0x14
#define UART16550_MSR 0x18
#define UART16550_SCR 0x1C

// LCR
#define UART16550_LCR_DLAB 0x80

// IER
#define UART16550_IER_ERBFI 0x01
#define UART16550_IER_ETBEI 0x02
#define UART16550_IER_ELSI  0x04
#define UART16550_IER_EDSSI 0x08

// IIR (interrupt id in bits [3:1], bit 0 is the active-low "no interrupt")
#define UART16550_IIR_NO_INT 0x01
#define UART16550_IIR_MSI    0x00
#define UART16550_IIR_THRI   0x02
#define UART16550_IIR_RDI    0x04
#define UART16550_IIR_RLSI   0x06
#define UART16550_IIR_FIFOEN 0xC0

// FCR
#define UART16550_FCR_FIFOEN     0x01
#define UART16550_FCR_CLEAR_RCVR 0x02
#define UART16550_FCR_CLEAR_XMIT 0x04

// MCR
#define UART16550_MCR_LOOP 0x10

// LSR
#define UART16550_LSR_DR   0x01
#define UART16550_LSR_THRE 0x20
#define UART16550_LSR_TEMT 0x40

class UART16550 : public AXI32_Slave {
private:
    static const u32 RX_FIFO_SIZE = 16;

    // Address of register index 0 = base + regOffset (device tree reg-offset).
    u32 regBase;

    // Registers that hold written values.
    u8 dll, dlm;
    u8 ier;
    u8 fcr;
    u8 lcr;
    u8 mcr;
    u8 scr;

    // Receiver FIFO.
    u8  rx_fifo[RX_FIFO_SIZE];
    u32 rx_head;
    u32 rx_tail;
    u32 rx_count;

    // Level-sensitive interrupt output (may be left unconnected).
    bool *irqLine;
    bool  consoleOpen;
    bool  escapePending;         // Ctrl-A seen, next byte is a console escape
    u32   pollCount;             // cycles since the last stdin poll
#ifndef OS_IS_WINDOWS
    struct termios savedTermios;
    int savedFlags;
    bool termiosSaved;
    bool flagsSaved;
    void (*oldSigtTou)(int);
    void (*oldSigtTtin)(int);
    void (*oldSigInt)(int);
    void (*oldSigTerm)(int);
#endif

    u32  readw(u32 addr);
    void writew(u32 data, u32 addr, u32 mask);
    u8   readb(u32 addr);
    void writeb(u8 data, u32 addr);

    u32  lsr() const;
    u32  iir() const;
    u32  msr() const;
    void pushRx(u8 byte);
    u8   popRx();
    bool rxEmpty() const { return rx_count == 0; }
    bool rxFull()  const { return rx_count >= RX_FIFO_SIZE; }
    void updateIRQ();
    void outputByte(u8 byte);
    void consoleByte(u8 byte);   // console input incl. the Ctrl-A escape

public:
    // base/len are the decoded AXI range chosen at instantiation.  regOffset is
    // the device tree "reg-offset" of this IP; it defaults to the CatnipSoC
    // value (0x1000) and may be overridden when wiring the UART elsewhere.
    UART16550(u32 base, u32 len, u32 regOffset = 0x1000);

    // Bind the interrupt line the UART drives.  A future SoC connects this to
    // the CPU interrupt controller (for CatnipSoC the DT puts the UART on
    // CPU IP2, i.e. MIPS32_Core::get_irq(2)).  The line is driven immediately
    // so the bound level reflects any already-pending interrupt condition.
    void setIRQLine(bool *line) { irqLine = line; updateIRQ(); }

    // Console input: switches stdin to raw, non-blocking mode.  Explicit so a
    // build that only polls the UART never hijacks the terminal.
    void openConsole();
    void closeConsole();

    // Poll stdin into the RX FIFO and refresh the interrupt line.  Call once
    // per emulated cycle (or at least regularly) while the console is open.
    void tick();
};

#endif
