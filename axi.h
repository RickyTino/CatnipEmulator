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
    virtual u32 read(u32 addr, u32 size);
    virtual void write(u32 data, u32 addr, u32 size, u32 mask = 0xFFFFFFFF);
}; 

class AXI32_Interconnect : public AXI32_Slave {
private:
    vector<AXI32_Slave*> slaves;

private:
    // u8 readb(u32 addr);
    // void writeb(u8 data, u32 addr);
    u32 readw(u32 addr);
    void writew(u32 data ,u32 addr, u32 mask);

public:
    AXI32_Interconnect(u32 base, u32 len);
    void addSlave(AXI32_Slave *slave);
    u32 read(u32 addr, u32 size);
    void write(u32 data, u32 addr, u32 size, u32 mask = 0xFFFFFFFF);

};

class AXI32_RAM : public AXI32_Slave {
private:
    vector<u32> ram;
    u32 addrMask;

private:
    // u8 readb(u32 addr);
    // void writeb(u8 data, u32 addr);
    u32 readw(u32 addr);
    void writew(u32 data, u32 addr, u32 mask);

public:
    AXI32_RAM(u32 base, u32 len, u32 width);
    void loadHex(string filename, u32 offset);
    // void loadBin(string filename, u32 offset);
    // u32 read(u32 addr, u32 size);
    // void write(u32 data, u32 addr, u32 size, u32 mask = 0xFFFFFFFF);

};

#endif
