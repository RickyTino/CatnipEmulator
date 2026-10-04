#include "axi.h"

AXI32_Slave::AXI32_Slave(u32 base, u32 len)
{
    setRange(base, len);
}

void AXI32_Slave::setRange(u32 base, u32 len)
{
    addrBase = base;
    addrLen = len;
}

bool AXI32_Slave::inRange(u32 addr)
{
    if (addrLen == 0) return true;
    return (addr >= addrBase && addr < (addrBase + addrLen));
}

bool AXI32_Slave::isMapped(u32 addr)
{
    return inRange(addr);
}

u32 AXI32_Slave::read(u32 addr, u32 size)
{
    if (size == 1) {
        return (u32)readb(addr);
    }
    else if (size == 2) {
        u32 tempaddr = addr & 0xFFFFFFFE;
        return bitConcat(readb(tempaddr | 1), 8, readb(tempaddr));
    }
    else {
        u32 tempaddr = addr & 0xFFFFFFFC;
        // u32 high = bitConcat(readb(tempaddr | 3), 8, readb(tempaddr | 2));
        // u32 low  = bitConcat(readb(tempaddr | 1), 8, readb(tempaddr | 0));
        // return bitConcat(high, 16, low);
        return readw(tempaddr);
    }
}

void AXI32_Slave::write(u32 data, u32 addr, u32 size, u32 mask)
{
    if (size == 1) {
        writeb(data, addr);
    }
    else if (size == 2) {
        u32 tempaddr = addr & 0xFFFFFFFE;
        writeb(bitPart(data, 15, 8), tempaddr | 1);
        writeb(bitPart(data,  7, 0), tempaddr | 0);
    }
    else {
        u32 tempaddr = addr & 0xFFFFFFFC;
        // writeb(bitPart(data, 31, 24), tempaddr | 3);
        // writeb(bitPart(data, 23, 16), tempaddr | 2);
        // writeb(bitPart(data, 15,  8), tempaddr | 1);
        // writeb(bitPart(data,  7,  0), tempaddr | 0);
        writew(data, tempaddr, mask);
    }
}

u8 AXI32_Slave::readb(u32 addr)
{
    u32 bytesel = addr & 0x3;
    u32 word_data = readw(addr);
    u8 res;
    res = bytesel == 0 ? (u8)bitPart(word_data,  7,  0) :
          bytesel == 1 ? (u8)bitPart(word_data, 15,  8) :
          bytesel == 2 ? (u8)bitPart(word_data, 23, 16) :
          (u8)bitPart(word_data, 31, 24);
    return res;
}

void AXI32_Slave::writeb(u8 data, u32 addr)
{
    u32 bytesel = addr & 0x3;
    u32 res = readw(addr);
    switch (bytesel) {
        case 0: res = bitReplace(res,  7,  0, data); break;
        case 1: res = bitReplace(res, 15,  8, data); break;
        case 2: res = bitReplace(res, 23, 16, data); break;
        case 3: res = bitReplace(res, 31, 24, data);
    }
    writew(res, addr, 0xFFFFFFFF);
}

AXI32_Interconnect::AXI32_Interconnect(u32 base, u32 len) :
AXI32_Slave(base, len)
{
    slaves.clear();
    unmapped_count = 0;
    unmapped_silenced = false;
}

void AXI32_Interconnect::addSlave(AXI32_Slave *slave)
{
    slaves.push_back(slave);
}

AXI32_Slave *AXI32_Interconnect::find(u32 addr)
{
    for (AXI32_Slave *i : slaves) {
        if (i->inRange(addr))
            return i;
    }
    return NULL;
}

bool AXI32_Interconnect::isMapped(u32 addr)
{
    return find(addr) != NULL;
}

u32 AXI32_Interconnect::read(u32 addr, u32 size)
{
    for (AXI32_Slave *i : slaves) {
        if (i->inRange(addr)) {
            return i->read(addr, size);
        }
    }
    reportUnmapped(addr, false);
    return 0;
}

void AXI32_Interconnect::write(u32 data, u32 addr, u32 size, u32 mask)
{
    for (AXI32_Slave *i : slaves) {
        if (i->inRange(addr)) {
            i->write(data, addr, size, mask);
            return;
        }
    }
    reportUnmapped(addr, true);
}

// A guest that runs off into unmapped space would otherwise spin forever on
// the 0 that read() returns, with nothing on the console to say why.  Guest
// visible behaviour is unchanged: this only tells the host what happened.
void AXI32_Interconnect::reportUnmapped(u32 addr, bool isWrite)
{
    for (u32 i = 0; i < unmapped_count; ++i) {
        if (unmapped_seen[i] == addr)
            return;                     // already reported this address
    }
    if (unmapped_count == UNMAPPED_REPORT_MAX) {
        if (!unmapped_silenced) {
            unmapped_silenced = true;
            cerr << EMU_TAG << "further unmapped accesses are not reported"
                 << endl;
        }
        return;
    }
    unmapped_seen[unmapped_count++] = addr;
    cerr << EMU_TAG << (isWrite ? "write to " : "read from ")
         << "unmapped address 0x" << hex << addr << dec
         << " (reads as 0, writes are dropped)" << endl;
}

// The interconnect has no register file of its own: readw/writew are never
// reached because read()/write() dispatch to the decoded slave first.
u32 AXI32_Interconnect::readw(u32) { return 0; }
void AXI32_Interconnect::writew(u32, u32, u32) { }

AXI32_RAM::AXI32_RAM(u32 base, u32 len, u32 width) :
AXI32_Slave(base, len),
ram(1 << (width - 2), 0)
// ram(bytesize >> 2)
{
//    ram.resize(bytesize >> 2);
    addrMask = (1 << width) - 1;
}

void AXI32_RAM::loadHex(string filename, u32 offset)
{
    ifstream inf;
    inf.open(filename, ios::in);
    u32 i = offset;
    u32 temp;
    while(!inf.eof() && i < ram.size()){
        inf >> hex >> temp;
        ram[i++] = temp;
    }
    inf.close();
}

void AXI32_RAM::loadBytes(const u8 *data, u32 size, u32 offset)
{
    for (u32 i = 0; i < size; ++i) {
        u32 byteAddr = offset + i;
        u32 wordAddr = byteAddr >> 2;
        if (wordAddr >= ram.size()) break;
        u32 lane = (byteAddr & 0x3) * 8;
        ram[wordAddr] = bitReplace(ram[wordAddr], lane + 7, lane, data[i]);
    }
}

bool AXI32_RAM::loadBin(string filename, u32 offset)
{
    ifstream inf(filename, ios::in | ios::binary);
    if (!inf)
        return false;

    inf.seekg(0, ios::end);
    streamsize size = inf.tellg();
    inf.seekg(0, ios::beg);
    if (size > 0) {
        vector<u8> buf((size_t)size);
        inf.read(reinterpret_cast<char *>(buf.data()), size);
        loadBytes(buf.data(), (u32)inf.gcount(), offset);
    }
    inf.close();
    return true;
}

// u32 AXI32_RAM::read(u32 addr, u32 size)
// {
//     u32 word_addr = (addr >> 2) % ram.size();
//     u32 bytesel = addr & 0x3;
//     u32 res;
//     u32 word_data = ram[word_addr];
//     if (size == 1) {
//         res = bytesel == 0 ? bitPart(word_data,  7,  0) :
//               bytesel == 1 ? bitPart(word_data, 15,  8) :
//               bytesel == 2 ? bitPart(word_data, 23, 16) :
//               bitPart(word_data, 31, 24);
//     }
//     else if (size == 2) {
//         res = bytesel == 0 ? bitPart(word_data, 15,  0) :
//               bytesel == 1 ? bitPart(word_data, 15,  0) :
//               bytesel == 2 ? bitPart(word_data, 31, 16) :
//               bitPart(word_data, 31, 16);
//     }
//     else 
//         res = word_data;
//     return res;
// }

// void AXI32_RAM::write(u32 data, u32 addr, u32 size, u32 mask)
// {
//     u32 word_addr = (addr >> 2) % ram.size();
//     u32 bytesel = addr & 0x3;
//     u32 res = ram[word_addr];

//     if (size == 1) {
//         switch (bytesel) {
//             case 0: res = bitReplace(res,  7,  0, data); break;
//             case 1: res = bitReplace(res, 15,  8, data); break;
//             case 2: res = bitReplace(res, 23, 16, data); break;
//             case 3: res = bitReplace(res, 31, 24, data);
//         }
//     }
//     else if (size == 2) {
//         switch (bytesel) {
//             case 0:
//             case 1: res = bitReplace(res, 15,  0, data); break;
//             case 2:
//             case 3: res = bitReplace(res, 31, 16, data);
//         }
//     }
//     else
//         res = data;
    
//     ram[word_addr] = (ram[word_addr] & ~mask) | (res & mask);
// }

// u8 AXI32_RAM::readb(u32 addr)
// {
//     u32 bytesel = addr & 0x3;
//     u32 word_data = readw(addr);
//     u8 res;
//     res = bytesel == 0 ? (u8)bitPart(word_data,  7,  0) :
//           bytesel == 1 ? (u8)bitPart(word_data, 15,  8) :
//           bytesel == 2 ? (u8)bitPart(word_data, 23, 16) :
//           (u8)bitPart(word_data, 31, 24);
//     return res;
// }

// void AXI32_RAM::writeb(u8 data, u32 addr)
// {
//     u32 bytesel = addr & 0x3;
//     u32 res = readw(addr);
//     switch (bytesel) {
//         case 0: res = bitReplace(res,  7,  0, data); break;
//         case 1: res = bitReplace(res, 15,  8, data); break;
//         case 2: res = bitReplace(res, 23, 16, data); break;
//         case 3: res = bitReplace(res, 31, 24, data);
//     }
//     writew(res, addr, 0xFFFFFFFF);
// }

u32 AXI32_RAM::readw(u32 addr)
{
    u32 ramAddr = (addr & addrMask) >> 2;
    return ram[ramAddr];
}

void AXI32_RAM::writew(u32 data, u32 addr, u32 mask)
{
    u32 ramAddr = (addr & addrMask) >> 2;
    ram[ramAddr] = (ram[ramAddr] & ~mask) | (data & mask);
}