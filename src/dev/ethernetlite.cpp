#include "ethernetlite.h"

#include <cstring>

EthernetLite::EthernetLite(u32 base, u32 len) : AXI32_Slave(base, len)
{
    regBase = base;

    // Reset: both directions idle.  TSR 0 passes both drivers' "buffer free"
    // test -- u-boot only wants BUSY clear (u-boot xilinx_emaclite.c:418), the
    // Linux driver wants BUSY and ACTIVE both clear (:329-343).
    memset(txbuf, 0, sizeof(txbuf));
    memset(mac, 0, sizeof(mac));
    tplr[0] = tplr[1] = 0;
    tsr[0]  = tsr[1]  = 0;

    memset(rxbuf, 0, sizeof(rxbuf));
    rsr[0] = rsr[1] = 0;
    nextRx = 0;
}

// Word helpers for the byte-addressed frame buffers.  "off" is the byte offset
// addressed by the CPU; like AXI32_RAM::readw, the word is selected by "off"
// with the low two bits ignored, and the lane selection is left to the generic
// AXI32_Slave::readb/writeb, which is why sub-word stores at unaligned
// addresses work (they read-modify-write through here).
static u32 readBytes(const u8 *buf, u32 off, u32 size)
{
    u32 wordOff = off & ~3u;
    u32 word = 0;
    for (u32 i = 0; i < 4; ++i) {
        if (wordOff + i < size)
            word |= (u32)buf[wordOff + i] << (8 * i);
    }
    return word;
}

static void writeBytes(u8 *buf, u32 off, u32 size, u32 data, u32 mask)
{
    u32 wordOff = off & ~3u;
    for (u32 i = 0; i < 4; ++i) {
        if (wordOff + i < size && (mask & (0xFFu << (8 * i))))
            buf[wordOff + i] = (u8)(data >> (8 * i));
    }
}

u32 EthernetLite::readw(u32 addr)
{
    u32 off = addr - regBase;

    switch (off) {
        case ETH_TSR_PING:  return tsr[0];
        case ETH_TPLR_PING: return tplr[0];
        case ETH_TSR_PONG:  return tsr[1];
        case ETH_TPLR_PONG: return tplr[1];
        case ETH_RSR_PING:  return rsr[0];
        case ETH_RSR_PONG:  return rsr[1];
        default: break;
    }

    if (off < ETH_BUFFER_OFFSET)
        return readBytes(txbuf[0], off, ETH_BUF_SIZE);
    if (off < ETH_RX_PINGBUF)
        return readBytes(txbuf[1], off - ETH_BUFFER_OFFSET, ETH_BUF_SIZE);
    if (off < ETH_RX_PONGBUF)
        return readBytes(rxbuf[0], off - ETH_RX_PINGBUF, ETH_BUF_SIZE);
    if (off < ETH_RX_PONGBUF + ETH_BUFFER_OFFSET)
        return readBytes(rxbuf[1], off - ETH_RX_PONGBUF, ETH_BUF_SIZE);

    // The MDIO registers (step 1.5) and the rest of the window read 0.
    return 0;
}

void EthernetLite::writew(u32 data, u32 addr, u32 mask)
{
    u32 off = addr - regBase;

    switch (off) {
        case ETH_TSR_PING:
            writeTsr(0, data);
            return;
        case ETH_TPLR_PING:
            tplr[0] = (tplr[0] & ~mask) | (data & mask);
            return;
        case ETH_TSR_PONG:
            writeTsr(1, data);
            return;
        case ETH_TPLR_PONG:
            tplr[1] = (tplr[1] & ~mask) | (data & mask);
            return;

        // Writing RSR is plain "store what was written": the drivers use it to
        // flush a buffer at start-up (writing RECV_IE) and to acknowledge a
        // frame (read, clear RECV_DONE, write back), and both are just stores.
        case ETH_RSR_PING:
            rsr[0] = (rsr[0] & ~mask) | (data & mask);
            return;
        case ETH_RSR_PONG:
            rsr[1] = (rsr[1] & ~mask) | (data & mask);
            return;
        default:
            break;
    }

    if (off < ETH_BUFFER_OFFSET)
        writeBytes(txbuf[0], off, ETH_BUF_SIZE, data, mask);
    else if (off < ETH_RX_PINGBUF)
        writeBytes(txbuf[1], off - ETH_BUFFER_OFFSET, ETH_BUF_SIZE, data, mask);
    else if (off < ETH_RX_PONGBUF)
        writeBytes(rxbuf[0], off - ETH_RX_PINGBUF, ETH_BUF_SIZE, data, mask);
    else if (off < ETH_RX_PONGBUF + ETH_BUFFER_OFFSET)
        writeBytes(rxbuf[1], off - ETH_RX_PONGBUF, ETH_BUF_SIZE, data, mask);
}

void EthernetLite::writeTsr(int which, u32 data)
{
    // BUSY|PROGRAM is the MAC programming sequence: the 6 address bytes sit at
    // the start of this direction's frame buffer, nothing is transmitted, and
    // the two bits must clear again because both drivers busy-wait on them
    // (u-boot xilinx_emaclite.c:339-343, Linux :486-491).  The drivers always
    // program exactly ETH_ALEN bytes; the length register is ignored.
    if ((data & ETH_TSR_PROG_MAC_ADDR) == ETH_TSR_PROG_MAC_ADDR) {
        memcpy(mac, txbuf[which], sizeof(mac));
        tsr[which] = data & ~ETH_TSR_PROG_MAC_ADDR;
        return;
    }

    u32 value = data;
    if (value & ETH_TSR_XMIT_BUSY) {
        u32 len = tplr[which] & ETH_TPLR_LENGTH_MASK;
        if (len > ETH_BUF_SIZE)
            len = ETH_BUF_SIZE;
        transmitFrame(txbuf[which], len);

        // The frame is on the wire, so BUSY clears while the other bits
        // (ACTIVE, IE) survive.  u-boot watches BUSY only; the Linux IRQ
        // handler retires the buffer on "BUSY clear && ACTIVE set" and clears
        // ACTIVE itself (xilinx_emaclite.c:662-677).
        value &= ~ETH_TSR_XMIT_BUSY;
    }
    tsr[which] = value;
}

void EthernetLite::transmitFrame(const u8 *frame, u32 len)
{
    (void)frame;
    (void)len;
}

bool EthernetLite::receiveFrame(const u8 *frame, u32 len)
{
    // No queue: offer the frame to the direction we are not pointing at, and
    // drop it when both still hold an unread frame.
    int which;
    if (!(rsr[nextRx] & ETH_RSR_RECV_DONE))
        which = nextRx;
    else if (!(rsr[nextRx ^ 1] & ETH_RSR_RECV_DONE))
        which = nextRx ^ 1;
    else
        return false;

    if (len > ETH_BUF_SIZE)
        len = ETH_BUF_SIZE;
    memset(rxbuf[which], 0, ETH_BUF_SIZE);
    memcpy(rxbuf[which], frame, len);

    // RECV_DONE goes up last: a driver that sees it must find the whole frame
    // there, including the EtherType at offset 12 that Linux derives the frame
    // length from.
    rsr[which] |= ETH_RSR_RECV_DONE;
    nextRx = which ^ 1;
    return true;
}
