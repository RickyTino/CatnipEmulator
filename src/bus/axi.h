#ifndef __AXI_H__
#define __AXI_H__

#include "common_defs.h"
#include "bit_utils.h"
// #include <fstream>

class AXI32_Slave {
private:
    u32 addrBase;
    u32 addrLen;

protected:
    virtual u8 readb(u32 addr);
    virtual void writeb(u8 data, u32 addr);
    virtual u32 readw(u32 addr) = 0;
    virtual void writew(u32 data ,u32 addr, u32 mask) = 0;

public:
    // AXI32_Slave();
    AXI32_Slave(u32 base, u32 len);
    void setRange(u32 base, u32 len);
    bool inRange(u32 addr);
    // True if this slave (or, for an interconnect, any slave behind it) decodes
    // the address -- used by the image loader to flag unmapped load addresses.
    virtual bool isMapped(u32 addr);
    virtual u32 read(u32 addr, u32 size);
    virtual void write(u32 data, u32 addr, u32 size, u32 mask = 0xFFFFFFFF);
}; 

class AXI32_Interconnect : public AXI32_Slave {
private:
    vector<AXI32_Slave*> slaves;

    // Accesses that decode to no slave: read() returns 0 and write() is
    // dropped, which looks exactly like a hang when a guest jumps into
    // unmapped space.  Report each distinct address once, then stay quiet.
    static const u32 UNMAPPED_REPORT_MAX = 8;
    u32  unmapped_seen[UNMAPPED_REPORT_MAX];
    u32  unmapped_count;
    bool unmapped_silenced;

private:
    u32 readw(u32 addr);
    void writew(u32 data ,u32 addr, u32 mask);
    void reportUnmapped(u32 addr, bool isWrite);

public:
    AXI32_Interconnect(u32 base, u32 len);
    void addSlave(AXI32_Slave *slave);
    bool isMapped(u32 addr);
    AXI32_Slave *find(u32 addr);
    u32 read(u32 addr, u32 size);
    void write(u32 data, u32 addr, u32 size, u32 mask = 0xFFFFFFFF);

};

class AXI32_RAM : public AXI32_Slave {
private:
    vector<u32> ram;
    u32 addrMask;

private:
    u32 readw(u32 addr);
    void writew(u32 data, u32 addr, u32 mask);

public:
    AXI32_RAM(u32 base, u32 len, u32 width);
    void loadHex(string filename, u32 offset);
    // Raw binary image (e.g. u-boot.bin / vmlinux) plus the in-memory variant
    // used to build an image programmatically.
    bool loadBin(string filename, u32 offset);
    void loadBytes(const u8 *data, u32 size, u32 offset);
    // void read(u32 addr, u32 size);
    // void write(u32 data, u32 addr, u32 size, u32 mask = 0xFFFFFFFF);

};

#endif
