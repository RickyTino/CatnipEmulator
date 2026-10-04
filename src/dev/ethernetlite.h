#ifndef __ETHERNETLITE_H__
#define __ETHERNETLITE_H__

#include "common_defs.h"
#include "axi.h"

// Xilinx xps-ethernetlite (10/100 MAC with ping/pong RX and TX buffers),
// modelled as an AXI32_Slave.  Linux reaches it through xilinx_emaclite
// ("xlnx,xps-ethernetlite-3.00.a"); u-boot uses the same driver family and
// the same register layout ("...-1.00.a"), so one model serves both.
//
// The class itself carries no absolute address.  As with the CatnipSoC device
// tree (arch/mips/boot/dts/catnipsoc/catnipsoc.dts):
//   ethernet@10e00000 { reg = <0x10e00000 0x10000>; ... }
// the instantiation picks the decoded range (base, len).
//
// Register offsets below are the ones the driver uses, relative to the start
// of the AXI window (linux xilinx_emaclite.c:30-46, u-boot same layout).  The
// second (pong) buffer of each direction sits XEL_BUFFER_OFFSET further up.

#define ETH_BUFFER_OFFSET 0x0800
// Each direction gets a 0x800 stride whose last 0x1C bytes carry that
// direction's length/status registers, so the frame buffer itself is 0x7E4.
// TX and RX buffers are the same size.
#define ETH_BUF_SIZE      0x07E4

#define ETH_TX_PINGBUF 0x0000
#define ETH_TX_PONGBUF (ETH_TX_PINGBUF + ETH_BUFFER_OFFSET)
#define ETH_RX_PINGBUF 0x1000
#define ETH_RX_PONGBUF (ETH_RX_PINGBUF + ETH_BUFFER_OFFSET)

#define ETH_MDIOADDR  0x07E4
#define ETH_MDIOWR    0x07E8
#define ETH_MDIORD    0x07EC
#define ETH_MDIOCTRL  0x07F0
#define ETH_TPLR_PING 0x07F4
#define ETH_GIER      0x07F8
#define ETH_TSR_PING  0x07FC
#define ETH_TPLR_PONG 0x0FF4
#define ETH_TSR_PONG  0x0FFC
#define ETH_RSR_PING  0x17FC
#define ETH_RSR_PONG  0x1FFC

// There is deliberately no "receive packet length" register here.  Neither
// driver reads one: u-boot's regs struct has no such field, and Linux only
// defines XEL_RPLR_OFFSET (0x100C) without ever using it.  What Linux reads at
// 0x100C is the frame's EtherType, i.e. RX buffer + 12 (XEL_HEADER_OFFSET), so
// that address is part of the receive buffer, not a register.

// MDIO Address Register (xilinx_emaclite.c:49-52)
#define ETH_MDIOADDR_REGADR_MASK  0x0000001F
#define ETH_MDIOADDR_PHYADR_MASK  0x000003E0
#define ETH_MDIOADDR_PHYADR_SHIFT 5
#define ETH_MDIOADDR_OP_MASK      0x00000400

// MDIO Write/Read Data Registers (xilinx_emaclite.c:55,58)
#define ETH_MDIOWR_WRDATA_MASK 0x0000FFFF
#define ETH_MDIORD_RDDATA_MASK 0x0000FFFF

// MDIO Control Register (xilinx_emaclite.c:61-62)
#define ETH_MDIOCTRL_MDIOSTS 0x00000001
#define ETH_MDIOCTRL_MDIOEN  0x00000008

// Global Interrupt Enable Register (xilinx_emaclite.c:65)
#define ETH_GIER_GIE 0x80000000

// Transmit Packet Length Register (xilinx_emaclite.c:82)
#define ETH_TPLR_LENGTH_MASK 0x0000FFFF

// Transmit Status Register (xilinx_emaclite.c:68-73)
#define ETH_TSR_XMIT_BUSY   0x00000001
#define ETH_TSR_PROGRAM     0x00000002
#define ETH_TSR_XMIT_IE     0x00000008
#define ETH_TSR_XMIT_ACTIVE 0x80000000
#define ETH_TSR_PROG_MAC_ADDR (ETH_TSR_XMIT_BUSY | ETH_TSR_PROGRAM)

// Receive Status Register (xilinx_emaclite.c:79-80)
#define ETH_RSR_RECV_DONE 0x00000001
#define ETH_RSR_RECV_IE   0x00000008

class EthernetLite : public AXI32_Slave {
private:
    // Address of the register window, i.e. of TX ping buffer offset 0.  Stored
    // because AXI32_Slave keeps its own base private (as UART16550 does).
    u32 regBase;

    // One TX instance per direction: [0] ping, [1] pong.  Each has its own
    // frame buffer, packet-length register and status register, which is what
    // lets both drivers alternate between the two.
    u8  txbuf[2][ETH_BUF_SIZE];
    u32 tplr[2];
    u32 tsr[2];

    // RX side of the same split.  A set RECV_DONE in rsr[i] means rxbuf[i]
    // holds a frame the driver has not taken yet.
    u8  rxbuf[2][ETH_BUF_SIZE];
    u32 rsr[2];
    int nextRx;                  // direction the next received frame is offered to

    // MAC address installed by the last BUSY|PROGRAM sequence, wire order.
    u8  mac[6];

    u32  readw(u32 addr);
    void writew(u32 data, u32 addr, u32 mask);
    void writeTsr(int which, u32 data);

protected:
    // Sink for a frame the MAC has finished sending.  Stage 1 has no host
    // backend yet, so this drops the frame; step 2.x overrides it with the
    // null/slirp one.  The buffer is only valid for the duration of the call.
    virtual void transmitFrame(const u8 *frame, u32 len);

public:
    // base/len are the decoded AXI range chosen at instantiation.
    EthernetLite(u32 base, u32 len);

    // MAC address most recently installed by the driver, in wire order.
    // Nothing in the send/receive path consumes it -- both drivers hand over
    // whole frames, with the source address already in them -- so it is state
    // for tests, and for a future option that pins a fixed address.
    const u8 *macAddress() const { return mac; }

    // Hand the MAC one received frame (a complete Ethernet frame, header
    // included), the way the wire would.  Step 2.x's backend calls this when
    // the host sends a packet, and the self-test calls it directly.
    // Returns false if both buffers still hold unread frames: the MAC has no
    // queue, so it drops the frame in that case.
    bool receiveFrame(const u8 *frame, u32 len);
};

#endif
