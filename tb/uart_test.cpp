// Standalone self-test for the minimal NS16550A model.
//
//   ./uart_test         deterministic register/loopback test (exit != 0 on fail)
//   ./uart_test echo    interactive console echo over the raw, non-blocking path
//
// It talks to the UART only through its AXI32_Slave read/write interface, the
// same way the CPU bus would.
#include "uart16550.h"
#include <cstring>

#ifndef OS_IS_WINDOWS
    #include <unistd.h>
#endif

// Instantiation-time wiring, matching the CatnipSoC device tree:
//   reg = <0x10400000 0x10000>, reg-offset = <0x1000>
static const u32 UART_BASE       = 0x10400000;
static const u32 UART_LEN        = 0x10000;
static const u32 UART_REG_OFFSET = 0x1000;
static const u32 UART_REG_BASE   = UART_BASE + UART_REG_OFFSET;

static int failures = 0;

static void check(bool cond, const char *name)
{
    cout << (cond ? "[PASS] " : "[FAIL] ") << name << endl;
    if (!cond)
        failures++;
}

static u32 rd(UART16550 &uart, u32 off) { return uart.read(UART_REG_BASE + off, 4); }
static void wr(UART16550 &uart, u32 off, u32 val) { uart.write(val, UART_REG_BASE + off, 4); }

static int run_selftest()
{
    UART16550 uart(UART_BASE, UART_LEN, UART_REG_OFFSET);
    bool irq = false;
    uart.setIRQLine(&irq);

    // Plain read/write registers.
    wr(uart, UART16550_LCR, 0x03);
    check(rd(uart, UART16550_LCR) == 0x03, "LCR read/write");
    wr(uart, UART16550_IER, 0x0F);
    check(rd(uart, UART16550_IER) == 0x0F, "IER read/write");
    wr(uart, UART16550_IER, 0x00);
    wr(uart, UART16550_MCR, 0x0B);
    check(rd(uart, UART16550_MCR) == 0x0B, "MCR read/write");
    wr(uart, UART16550_SCR, 0xA5);
    check(rd(uart, UART16550_SCR) == 0xA5, "SCR read/write");

    // DLAB selects the divisor latches at the RBR/IER addresses.
    wr(uart, UART16550_LCR, UART16550_LCR_DLAB | 0x03);
    wr(uart, UART16550_DLL, 0x1B);
    wr(uart, UART16550_DLM, 0x01);
    check(rd(uart, UART16550_DLL) == 0x1B, "DLL via DLAB");
    check(rd(uart, UART16550_DLM) == 0x01, "DLM via DLAB");
    wr(uart, UART16550_LCR, 0x03);

    // Line status: transmitter always ready, receiver empty.
    u32 lsr = rd(uart, UART16550_LSR);
    check((lsr & (UART16550_LSR_THRE | UART16550_LSR_TEMT)) ==
          (UART16550_LSR_THRE | UART16550_LSR_TEMT), "LSR THRE|TEMT set");
    check((lsr & UART16550_LSR_DR) == 0, "LSR DR clear when empty");

    // Interrupt identification follows IER.
    wr(uart, UART16550_IER, UART16550_IER_ETBEI);
    u32 iir = rd(uart, UART16550_IIR);
    check((iir & UART16550_IIR_NO_INT) == 0, "IIR pending with IER.ETBEI");
    check((iir & 0x0E) == UART16550_IIR_THRI, "IIR id = THR empty");
    wr(uart, UART16550_IER, 0x00);
    check((rd(uart, UART16550_IIR) & UART16550_IIR_NO_INT) != 0,
          "IIR no interrupt with IER=0");
    check(!irq, "IRQ line low with IER=0");

    // Loopback: transmitted bytes come back through the receiver FIFO.
    wr(uart, UART16550_MCR, UART16550_MCR_LOOP);
    wr(uart, UART16550_IER, UART16550_IER_ERBFI);
    wr(uart, UART16550_THR, 'h');
    check((rd(uart, UART16550_LSR) & UART16550_LSR_DR) != 0,
          "loopback sets LSR.DR");
    check(irq, "loopback asserts IRQ with IER.ERBFI");
    wr(uart, UART16550_THR, 'i');
    check(rd(uart, UART16550_RBR) == 'h', "loopback RX byte 1");
    check(rd(uart, UART16550_RBR) == 'i', "loopback RX byte 2");
    check((rd(uart, UART16550_LSR) & UART16550_LSR_DR) == 0,
          "LSR.DR clears after draining");
    check(!irq, "IRQ deasserts after draining");
    wr(uart, UART16550_IER, 0x00);
    wr(uart, UART16550_MCR, 0x00);

    // Real transmission to stdout.
    cout << "TX marker: ";
    wr(uart, UART16550_THR, 'O');
    wr(uart, UART16550_THR, 'K');
    wr(uart, UART16550_THR, '\n');

    if (failures == 0)
        cout << "uart selftest: ALL PASS" << endl;
    else
        cout << "uart selftest: " << failures << " FAILED" << endl;
    return failures;
}

static int run_echo()
{
    UART16550 uart(UART_BASE, UART_LEN, UART_REG_OFFSET);
    bool irq = false;
    bool sawIrq = false;
    uart.setIRQLine(&irq);

    uart.openConsole();
    wr(uart, UART16550_IER, UART16550_IER_ERBFI);  // enable RX interrupts

    cout << "UART echo test. Type characters; Ctrl-A x quits (Ctrl-D/ESC too).\r\n"
         << flush;

    bool quit = false;
    while (!quit) {
        uart.tick();
        if (irq)
            sawIrq = true;

        while (rd(uart, UART16550_LSR) & UART16550_LSR_DR) {
            u8 ch = (u8)rd(uart, UART16550_RBR);
            if (ch == 0x04 || ch == 0x1B) {
                quit = true;
                break;
            }
            if (ch == '\r') {
                wr(uart, UART16550_THR, '\r');
                wr(uart, UART16550_THR, '\n');
            } else {
                wr(uart, UART16550_THR, ch);
            }
        }
#ifndef OS_IS_WINDOWS
        usleep(1000);
#endif
    }

    uart.closeConsole();
    cout << "\r\nuart echo done (RX interrupt seen: "
         << (sawIrq ? "yes" : "no") << ")\r\n";
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "echo") == 0)
        return run_echo();
    return run_selftest() == 0 ? 0 : 1;
}
