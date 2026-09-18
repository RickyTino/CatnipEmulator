#include "uart16550.h"

#include <cstdlib>

#ifdef OS_IS_WINDOWS
    #include <conio.h>
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <signal.h>

// Ctrl-C has to terminate the emulator even though the console runs in raw
// mode.  The handler restores the terminal before dying, otherwise the shell
// would be left in raw mode.
static UART16550 *g_consoleOwner = NULL;

static void uartConsoleSignal(int sig)
{
    if (g_consoleOwner)
        g_consoleOwner->closeConsole();
    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

UART16550::UART16550(u32 base, u32 len, u32 regOffset) :
AXI32_Slave(base, len)
{
    regBase = base + regOffset;

    dll = dlm = 0;
    ier = fcr = lcr = mcr = scr = 0;

    rx_head = rx_tail = rx_count = 0;

    irqLine = NULL;
    consoleOpen = false;
    escapePending = false;
    pollCount = 0;
#ifndef OS_IS_WINDOWS
    savedFlags = 0;
    termiosSaved = false;
    flagsSaved = false;
    oldSigInt = SIG_DFL;
    oldSigTerm = SIG_DFL;
#endif
}

u32 UART16550::lsr() const
{
    // Transmission is instantaneous, so THRE and TEMT are always set.  Only
    // the receiver data-ready bit changes; line-status error bits stay 0.
    u32 res = UART16550_LSR_THRE | UART16550_LSR_TEMT;
    if (!rxEmpty())
        res |= UART16550_LSR_DR;
    return res;
}

u32 UART16550::msr() const
{
    // In loopback the modem status inputs mirror the modem control outputs.
    if (mcr & UART16550_MCR_LOOP)
        return (u32)((mcr & 0x0F) << 4);
    return 0;
}

u32 UART16550::iir() const
{
    // Interrupts are reported purely as a function of the current registers,
    // which is enough for the 8250 startup self-tests and the RX data path.
    u32 fifo = (fcr & UART16550_FCR_FIFOEN) ? UART16550_IIR_FIFOEN : 0;

    if (!rxEmpty() && (ier & UART16550_IER_ERBFI))
        return fifo | UART16550_IIR_RDI;
    if ((ier & UART16550_IER_ETBEI) && (lsr() & UART16550_LSR_THRE))
        return fifo | UART16550_IIR_THRI;
    return fifo | UART16550_IIR_NO_INT;
}

void UART16550::updateIRQ()
{
    if (irqLine)
        *irqLine = (iir() & UART16550_IIR_NO_INT) == 0;
}

void UART16550::outputByte(u8 byte)
{
    cout.put((char)byte);
    cout.flush();
}

void UART16550::pushRx(u8 byte)
{
    if (rxFull())
        return;  // overrun: drop, matching a FIFO with no error reporting
    rx_fifo[rx_tail] = byte;
    rx_tail = (rx_tail + 1) % RX_FIFO_SIZE;
    rx_count++;
    updateIRQ();
}

u8 UART16550::popRx()
{
    if (rxEmpty())
        return 0;
    u8 byte = rx_fifo[rx_head];
    rx_head = (rx_head + 1) % RX_FIFO_SIZE;
    rx_count--;
    updateIRQ();
    return byte;
}

u32 UART16550::readw(u32 addr)
{
    switch (addr - regBase) {
        case UART16550_RBR:
            if (lcr & UART16550_LCR_DLAB)
                return dll;
            return popRx();
        case UART16550_IER:
            if (lcr & UART16550_LCR_DLAB)
                return dlm;
            return ier;
        case UART16550_IIR: return iir();
        case UART16550_LCR: return lcr;
        case UART16550_MCR: return mcr;
        case UART16550_LSR: return lsr();
        case UART16550_MSR: return msr();
        case UART16550_SCR: return scr;
        default:            return 0;
    }
}

void UART16550::writew(u32 data, u32 addr, u32 /*mask*/)
{
    u8 val = (u8)(data & 0xFF);

    switch (addr - regBase) {
        case UART16550_THR:
            if (lcr & UART16550_LCR_DLAB) {
                dll = val;
            } else if (mcr & UART16550_MCR_LOOP) {
                pushRx(val);
            } else {
                outputByte(val);
            }
            break;

        case UART16550_IER:
            if (lcr & UART16550_LCR_DLAB)
                dlm = val;
            else
                ier = val;
            break;

        case UART16550_FCR:
            fcr = val;
            if (val & UART16550_FCR_CLEAR_RCVR) {
                rx_head = rx_tail = rx_count = 0;
                fcr &= ~UART16550_FCR_CLEAR_RCVR;  // reset bits self-clear
            }
            fcr &= ~UART16550_FCR_CLEAR_XMIT;
            break;

        case UART16550_LCR: lcr = val; break;
        case UART16550_MCR: mcr = val; break;
        case UART16550_SCR: scr = val; break;

        default: break;
    }

    updateIRQ();
}

u8 UART16550::readb(u32 addr)
{
    u32 word = readw(addr & ~0x3u);
    return (u8)(word >> (8 * (addr & 0x3)));
}

void UART16550::writeb(u8 data, u32 addr)
{
    // Avoid the base class read-modify-write: reading RBR would consume a
    // received byte.  All UART registers are word aligned, so the byte value
    // simply goes in bits [7:0].
    writew(data, addr & ~0x3u, 0xFF);
}

void UART16550::openConsole()
{
    if (consoleOpen)
        return;

#ifndef OS_IS_WINDOWS
    // Job control: tcsetattr() raises SIGTTOU and read() raises SIGTTIN when
    // the process is not the terminal's foreground process group.  Ignore both
    // while the console is open so a backgrounded emulator is not stopped.
    oldSigtTou = signal(SIGTTOU, SIG_IGN);
    oldSigtTtin = signal(SIGTTIN, SIG_IGN);

    // Quitting is via the Ctrl-A escape (see consoleByte); SIGINT/SIGTERM are
    // still handled so an external kill restores the terminal on the way out.
    g_consoleOwner = this;
    oldSigInt  = signal(SIGINT,  uartConsoleSignal);
    oldSigTerm = signal(SIGTERM, uartConsoleSignal);

    // Only switch the terminal to raw mode when we actually own the
    // foreground; a background process must leave the terminal alone.
    if (tcgetpgrp(STDIN_FILENO) == getpgrp()) {
        termiosSaved = (tcgetattr(STDIN_FILENO, &savedTermios) == 0);
        if (termiosSaved) {
            struct termios raw = savedTermios;
            cfmakeraw(&raw);           // ISIG stays cleared: Ctrl-C goes to the guest
            raw.c_cc[VMIN]  = 0;
            raw.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        }
    }

    // A pipe or redirected input is still read non-blocking.
    savedFlags = fcntl(STDIN_FILENO, F_GETFL, 0);
    flagsSaved = (savedFlags != -1);
    if (flagsSaved)
        fcntl(STDIN_FILENO, F_SETFL, savedFlags | O_NONBLOCK);
#endif

    consoleOpen = true;
}

void UART16550::closeConsole()
{
    if (!consoleOpen)
        return;

#ifndef OS_IS_WINDOWS
    if (termiosSaved)
        tcsetattr(STDIN_FILENO, TCSANOW, &savedTermios);
    if (flagsSaved)
        fcntl(STDIN_FILENO, F_SETFL, savedFlags);
    signal(SIGINT,  oldSigInt);
    signal(SIGTERM, oldSigTerm);
    signal(SIGTTOU, oldSigtTou);
    signal(SIGTTIN, oldSigtTtin);
    g_consoleOwner = NULL;
#endif

    consoleOpen = false;
}

// Ctrl-A is the console escape prefix (QEMU style): the byte after it selects
// a host action instead of being delivered to the guest.
static const u8 CONSOLE_ESCAPE = 0x01;

void UART16550::consoleByte(u8 byte)
{
    if (escapePending) {
        escapePending = false;
        switch (byte) {
            case CONSOLE_ESCAPE:            // Ctrl-A Ctrl-A -> literal Ctrl-A
                pushRx(CONSOLE_ESCAPE);
                break;
            case 'x':
            case 'X':
                closeConsole();
                exit(0);                    // does not return, so no break needed
            case 'h':
            case '?':
                cout << "\r\n"
                     << EMU_TAG << "Ctrl-A x      : quit\r\n"
                     << EMU_TAG << "Ctrl-A h      : this help\r\n"
                     << EMU_TAG << "Ctrl-A Ctrl-A : send a literal Ctrl-A\r\n"
                     << flush;
                break;
            default:
                break;                      // unknown escape: ignore
        }
        return;
    }

    if (byte == CONSOLE_ESCAPE) {
        escapePending = true;
        return;
    }

    pushRx(byte);
}

void UART16550::tick()
{
    if (!consoleOpen || rxFull())
        return;

    // Reading stdin is a syscall; doing it every emulated cycle dominates
    // runtime.  Poll every ~1k cycles instead - still imperceptible for input
    // but orders of magnitude faster.
    if (++pollCount < 1024)
        return;
    pollCount = 0;

#ifndef OS_IS_WINDOWS
    // Read only as much as the RX FIFO can take; bytes left in the kernel
    // buffer are picked up on a later tick, so nothing is dropped.
    u32 space = RX_FIFO_SIZE - rx_count;
    u8 buf[RX_FIFO_SIZE];
    ssize_t n = ::read(STDIN_FILENO, buf, space);
    for (ssize_t i = 0; i < n; ++i)
        consoleByte(buf[i]);
#else
    while (!rxFull() && _kbhit())
        consoleByte((u8)_getch());
#endif
}
