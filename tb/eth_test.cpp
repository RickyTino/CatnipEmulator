// Standalone self-test for the xps-ethernetlite model.  Step 1.1 covers the TX
// path only; RX, MAC programming, interrupts and MDIO are added by later steps.
//
//   ./eth_test
//
// It drives the device only through its AXI32_Slave read/write interface, the
// same way the CPU bus would, and plays back the register sequences the two
// drivers use (u-boot and Linux xilinx_emaclite).
#include "ethernetlite.h"
#include <cstring>

// Instantiation-time wiring, matching the CatnipSoC device tree:
//   ethernet@10e00000 { reg = <0x10e00000 0x10000>; }
static const u32 ETH_BASE = 0x10E00000;
static const u32 ETH_LEN  = 0x10000;

static int failures = 0;

static void check(bool cond, const char *name)
{
    cout << (cond ? "[PASS] " : "[FAIL] ") << name << endl;
    if (!cond)
        failures++;
}

// Records what the MAC hands to the host backend.  Stage 1 leaves the default
// sink in place (the frame is dropped), so the test has to catch it here.
class TestEth : public EthernetLite {
public:
    u8  frame[ETH_BUF_SIZE];
    u32 frameLen;
    u32 frameCount;

    TestEth() : EthernetLite(ETH_BASE, ETH_LEN), frameLen(0), frameCount(0)
    {
        memset(frame, 0, sizeof(frame));
    }

protected:
    void transmitFrame(const u8 *sent, u32 len)
    {
        if (len > sizeof(frame))
            len = sizeof(frame);
        memcpy(frame, sent, len);
        frameLen = len;
        frameCount++;
    }
};

static u32 rd(TestEth &eth, u32 off) { return eth.read(ETH_BASE + off, 4); }
static void wr(TestEth &eth, u32 off, u32 val) { eth.write(val, ETH_BASE + off, 4); }

// Stage a frame the way xemaclite_alignedwrite does: whole words, then the
// trailing bytes.
static void writeFrame(TestEth &eth, u32 bufOff, const u8 *data, u32 len)
{
    u32 i = 0;
    for (; i + 4 <= len; i += 4) {
        u32 word = (u32)data[i] | ((u32)data[i + 1] << 8) |
                   ((u32)data[i + 2] << 16) | ((u32)data[i + 3] << 24);
        wr(eth, bufOff + i, word);
    }
    for (; i < len; ++i)
        eth.write(data[i], ETH_BASE + bufOff + i, 1);
}

static int run_selftest()
{
    TestEth eth;

    u8 frameA[60];
    u8 frameB[46];
    for (u32 i = 0; i < sizeof(frameA); ++i)
        frameA[i] = (u8)(0xA0 + i);
    for (u32 i = 0; i < sizeof(frameB); ++i)
        frameB[i] = (u8)(0xB0 + i);

    // Reset state: both directions must look free to both drivers, i.e. BUSY
    // (u-boot) and BUSY|ACTIVE (Linux) clear.
    check(rd(eth, ETH_TSR_PING) == 0, "reset TSR ping = 0");
    check(rd(eth, ETH_TSR_PONG) == 0, "reset TSR pong = 0");
    check(eth.frameCount == 0, "no frame sent before any trigger");

    // --- u-boot style: stage in ping, trigger by setting BUSY alone ---------
    wr(eth, ETH_TPLR_PING, sizeof(frameA));
    check(rd(eth, ETH_TPLR_PING) == sizeof(frameA), "TPLR ping read/write");
    writeFrame(eth, ETH_TX_PINGBUF, frameA, sizeof(frameA));

    u32 stored = rd(eth, ETH_TX_PINGBUF);
    u32 expect = (u32)frameA[0] | ((u32)frameA[1] << 8) |
                 ((u32)frameA[2] << 16) | ((u32)frameA[3] << 24);
    check(stored == expect, "ping buffer reads back the staged frame");

    wr(eth, ETH_TSR_PING, ETH_TSR_XMIT_BUSY);
    check(eth.frameCount == 1, "u-boot style: BUSY alone sends a frame");
    check(eth.frameLen == sizeof(frameA), "u-boot style: length comes from TPLR");
    check(memcmp(eth.frame, frameA, sizeof(frameA)) == 0,
          "u-boot style: frame payload intact");
    check(rd(eth, ETH_TSR_PING) == 0,
          "u-boot style: BUSY cleared, buffer free again");

    // --- Linux style: stage in pong, trigger with BUSY|ACTIVE --------------
    wr(eth, ETH_TPLR_PONG, sizeof(frameB));
    writeFrame(eth, ETH_TX_PONGBUF, frameB, sizeof(frameB));

    wr(eth, ETH_TSR_PONG, ETH_TSR_XMIT_BUSY | ETH_TSR_XMIT_ACTIVE);
    check(eth.frameCount == 2, "Linux style: BUSY|ACTIVE sends a frame");
    check(eth.frameLen == sizeof(frameB), "Linux style: length comes from TPLR");
    check(memcmp(eth.frame, frameB, sizeof(frameB)) == 0,
          "Linux style: frame payload intact");
    check(rd(eth, ETH_TSR_PONG) == ETH_TSR_XMIT_ACTIVE,
          "Linux style: BUSY cleared, ACTIVE kept");

    // The two directions are independent hardware.
    check(rd(eth, ETH_TSR_PING) == 0, "ping status untouched by pong transfer");

    // The Linux IRQ handler retires the buffer by clearing ACTIVE itself.
    wr(eth, ETH_TSR_PONG, rd(eth, ETH_TSR_PONG) & ~ETH_TSR_XMIT_ACTIVE);
    check(rd(eth, ETH_TSR_PONG) == 0,
          "Linux style: buffer free after the handler clears ACTIVE");

    // The packet length is sampled from TPLR at trigger time.
    wr(eth, ETH_TPLR_PING, 20);
    wr(eth, ETH_TSR_PING, ETH_TSR_XMIT_BUSY);
    check(eth.frameLen == 20, "length follows a re-written TPLR");

    // MAC programming (BUSY|PROGRAM) stages the 6-byte address instead of a
    // frame, and the two bits must clear or both drivers spin forever.
    const u8 mac[6] = { 0x02, 0x00, 0x5A, 0x11, 0x22, 0x33 };
    writeFrame(eth, ETH_TX_PINGBUF, mac, sizeof(mac));

    u32 countBefore = eth.frameCount;
    wr(eth, ETH_TSR_PING, ETH_TSR_PROG_MAC_ADDR);
    check(eth.frameCount == countBefore, "BUSY|PROGRAM does not send a frame");
    check(rd(eth, ETH_TSR_PING) == 0, "busy wait on BUSY|PROGRAM terminates");
    check(memcmp(eth.macAddress(), mac, sizeof(mac)) == 0,
          "BUSY|PROGRAM installs the MAC address");

    // The driver alternates between the two buffers, so pong must program too.
    const u8 macPong[6] = { 0x02, 0x00, 0x5A, 0x44, 0x55, 0x66 };
    writeFrame(eth, ETH_TX_PONGBUF, macPong, sizeof(macPong));
    wr(eth, ETH_TSR_PONG, ETH_TSR_PROG_MAC_ADDR);
    check(memcmp(eth.macAddress(), macPong, sizeof(macPong)) == 0,
          "pong direction programs the MAC too");

    // Sub-word stores must not clobber their neighbours: the generic
    // AXI32_Slave::writeb does read-modify-write through readw/writew.
    wr(eth, ETH_TX_PINGBUF, 0x44332211);
    eth.write((u32)0x99, ETH_BASE + ETH_TX_PINGBUF + 1, 1);
    check(rd(eth, ETH_TX_PINGBUF) == 0x44339911, "byte store merges into the word");

    // --- RX path -----------------------------------------------------------
    u8 rxA[64];
    for (u32 i = 0; i < sizeof(rxA); ++i)
        rxA[i] = (u8)(0x10 + i);
    rxA[12] = 0x08;                       // EtherType = 0x0800 (IPv4)
    rxA[13] = 0x00;

    // Driver start-up flushes each buffer by writing RECV_IE to its RSR.
    wr(eth, ETH_RSR_PING, ETH_RSR_RECV_IE);
    wr(eth, ETH_RSR_PONG, ETH_RSR_RECV_IE);
    check(rd(eth, ETH_RSR_PING) == ETH_RSR_RECV_IE, "RSR read/write (start-up flush)");
    check((rd(eth, ETH_RSR_PONG) & ETH_RSR_RECV_DONE) == 0,
          "flush leaves RECV_DONE clear");

    check(eth.receiveFrame(rxA, sizeof(rxA)), "receiveFrame accepts a frame");
    check(rd(eth, ETH_RSR_PING) == (ETH_RSR_RECV_IE | ETH_RSR_RECV_DONE),
          "ping RSR: RECV_DONE set, RECV_IE kept");
    check((rd(eth, ETH_RSR_PONG) & ETH_RSR_RECV_DONE) == 0,
          "pong status untouched by the ping frame");

    // The frame is readable, and Linux derives the packet length from the
    // EtherType at RX buffer + 12 -- the address 0x100C, which must read as
    // frame data and not as a register (see the header note).
    u32 raw = rd(eth, ETH_RX_PINGBUF + 12);
    u32 ethertype = ((raw & 0xFF) << 8) | ((raw >> 8) & 0xFF);   // ntohl() >> 16
    check(ethertype == 0x0800, "EtherType readable at RX buffer + 12");

    u32 rxStored = rd(eth, ETH_RX_PINGBUF + 16);
    u32 rxExpect = (u32)rxA[16] | ((u32)rxA[17] << 8) |
                   ((u32)rxA[18] << 16) | ((u32)rxA[19] << 24);
    check(rxStored == rxExpect, "RX buffer holds the injected frame");

    // The driver acknowledges by clearing RECV_DONE; RECV_IE is its own bit.
    wr(eth, ETH_RSR_PING, rd(eth, ETH_RSR_PING) & ~ETH_RSR_RECV_DONE);
    check(rd(eth, ETH_RSR_PING) == ETH_RSR_RECV_IE,
          "RECV_DONE clears on ack, RECV_IE survives");

    // Frames alternate between the two buffers, and a freed buffer is reused.
    u8 rxB[46];
    for (u32 i = 0; i < sizeof(rxB); ++i)
        rxB[i] = (u8)(0x20 + i);
    check(eth.receiveFrame(rxB, sizeof(rxB)), "second frame accepted");
    check((rd(eth, ETH_RSR_PONG) & ETH_RSR_RECV_DONE) != 0, "second frame goes to pong");
    check((rd(eth, ETH_RSR_PING) & ETH_RSR_RECV_DONE) == 0, "ping left alone");

    check(eth.receiveFrame(rxA, sizeof(rxA)), "third frame refills ping");
    check(!eth.receiveFrame(rxA, sizeof(rxA)),
          "frame dropped when both buffers are full");

    if (failures == 0)
        cout << "eth selftest: ALL PASS" << endl;
    else
        cout << "eth selftest: " << failures << " FAILED" << endl;
    return failures;
}

int main()
{
    return run_selftest() == 0 ? 0 : 1;
}
