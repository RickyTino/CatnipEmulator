#include "image_loader.h"
#include <cstdio>

// ELF / uImage structs are little- and big-endian respectively, independent of
// the host, so decode by hand.
static u16 rd16le(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }
static u32 rd32le(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}
static u32 rd32be(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static string hexstr(u32 v)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%08X", v);
    return string(buf);
}

// MIPS convention: the ELF/uImage "address" is a KSEG0/KSEG1 virtual address;
// the physical address is the low 29 bits.
static u32 toPhys(u32 vaddr)
{
    return vaddr & 0x1FFFFFFF;
}

static bool readFile(const string &path, vector<u8> &out)
{
    ifstream inf(path, ios::in | ios::binary);
    if (!inf)
        return false;
    inf.seekg(0, ios::end);
    streamsize size = inf.tellg();
    inf.seekg(0, ios::beg);
    if (size > 0) {
        out.resize((size_t)size);
        inf.read(reinterpret_cast<char *>(out.data()), size);
        out.resize((size_t)inf.gcount());
    }
    inf.close();
    return true;
}

static void busWrite(AXI32_Slave &bus, u32 addr, const u8 *data, u32 size)
{
    u32 i = 0;
    for (; i + 4 <= size; i += 4) {
        u32 w = (u32)data[i] | ((u32)data[i + 1] << 8) |
                ((u32)data[i + 2] << 16) | ((u32)data[i + 3] << 24);
        bus.write(w, addr + i, 4);
    }
    for (; i < size; ++i)
        bus.write(data[i], addr + i, 1);
}

static void busZero(AXI32_Slave &bus, u32 addr, u32 size)
{
    u32 i = 0;
    for (; i + 4 <= size; i += 4)
        bus.write(0, addr + i, 4);
    for (; i < size; ++i)
        bus.write(0, addr + i, 1);
}

// Place one region, checking that the whole [phys, phys+size) range is decoded.
static bool place(AXI32_Slave &bus, ImageLoadInfo &info,
                  u32 phys, const u8 *data, u32 filesz, u32 memsz)
{
    if (filesz > 0 &&
        (!bus.isMapped(phys) ||
         (u64)phys + filesz - 1 > 0xFFFFFFFFull ||
         !bus.isMapped(phys + filesz - 1))) {
        info.ok = false;
        info.error = "load address " + hexstr(phys) + " (+" + hexstr(filesz) +
                     ") is not backed by any mapped region";
        return false;
    }
    busWrite(bus, phys, data, filesz);
    if (memsz > filesz)
        busZero(bus, phys + filesz, memsz - filesz);   // zero .bss
    ImageSegment seg = { phys, filesz };
    info.segments.push_back(seg);
    return true;
}

static bool loadElf(AXI32_Slave &bus, const vector<u8> &buf, ImageLoadInfo &info)
{
    // Fixed ELF32 header is 52 bytes; validate before touching any field.
    if (buf.size() < 52) {
        info.error = "truncated ELF header";
        return false;
    }
    if (buf[4] != 1 || buf[5] != 1) {          // EI_CLASS=ELFCLASS32, EI_DATA=LSB
        info.error = "only ELF32 little-endian is supported";
        return false;
    }
    if (rd16le(&buf[18]) != 8) {               // e_machine = EM_MIPS
        info.error = "not a MIPS ELF";
        return false;
    }

    info.entry = rd32le(&buf[24]);             // e_entry (virtual, as-is)
    u32 e_phoff = rd32le(&buf[28]);
    u16 e_phentsize = rd16le(&buf[42]);
    u16 e_phnum = rd16le(&buf[44]);

    // e_phentsize must be large enough for the fields we read, and the whole
    // program-header table must fit.  Compute in 64 bits so a hostile/huge
    // e_phnum cannot wrap the bound check.
    if (e_phnum > 0 && e_phentsize < 32) {
        info.error = "bad ELF program-header entry size";
        return false;
    }
    if ((u64)e_phoff + (u64)e_phnum * e_phentsize > buf.size()) {
        info.error = "truncated ELF program headers";
        return false;
    }

    bool any = false;
    for (u16 i = 0; i < e_phnum; ++i) {
        const u8 *ph = &buf[e_phoff + (u32)i * e_phentsize];
        if (rd32le(ph + 0) != 1)               // PT_LOAD
            continue;

        u32 p_offset = rd32le(ph + 4);
        u32 p_vaddr  = rd32le(ph + 8);
        u32 p_paddr  = rd32le(ph + 12);
        u32 p_filesz = rd32le(ph + 16);
        u32 p_memsz  = rd32le(ph + 20);

        if (p_memsz < p_filesz) {
            info.error = "malformed ELF segment (memsz < filesz)";
            return false;
        }
        if ((u64)p_offset + p_filesz > buf.size()) {
            info.error = "truncated ELF segment";
            return false;
        }

        u32 phys = toPhys(p_paddr ? p_paddr : p_vaddr);
        if (!place(bus, info, phys, &buf[p_offset], p_filesz, p_memsz))
            return false;
        any = true;
    }
    if (!any) {
        info.error = "ELF has no PT_LOAD segment";
        return false;
    }
    return true;
}

static bool loadUImage(AXI32_Slave &bus, const vector<u8> &buf, ImageLoadInfo &info)
{
    if (buf.size() < 64) {
        info.error = "truncated uImage header";
        return false;
    }
    u32 ih_size = rd32be(&buf[12]);
    u32 ih_load = rd32be(&buf[16]);
    u32 ih_ep   = rd32be(&buf[20]);
    u8  ih_comp = buf[31];

    if (ih_comp != 0) {
        info.error = "compressed uImage is not supported (ih_comp=" +
                     hexstr(ih_comp) + ")";
        return false;
    }
    if ((u64)64 + ih_size > buf.size()) {
        info.error = "truncated uImage payload";
        return false;
    }

    info.entry = ih_ep;
    return place(bus, info, toPhys(ih_load), &buf[64], ih_size, ih_size);
}

static bool isElfImage(const vector<u8> &b)
{
    return b.size() >= 4 && b[0] == 0x7F && b[1] == 'E' &&
           b[2] == 'L' && b[3] == 'F';
}

static bool isUImage(const vector<u8> &b)
{
    return b.size() >= 4 && rd32be(&b[0]) == 0x27051956;
}

// Stage the whole file (headers included) at a chosen *physical* address; a
// bootloader such as u-boot can then bootm/bootelf it from there.
static bool stageAt(AXI32_Slave &bus, ImageLoadInfo &info,
                    const vector<u8> &buf, u32 addr)
{
    if (!bus.isMapped(addr) && bus.isMapped(addr & 0x1FFFFFFF)) {
        info.error = "load address " + hexstr(addr) +
                     " is not mapped - it looks like a KSEG address; use the "
                     "physical one (" + hexstr(addr & 0x1FFFFFFF) + ")";
        return false;
    }
    if (!place(bus, info, addr, buf.data(), (u32)buf.size(), (u32)buf.size()))
        return false;
    info.entry = addr;
    return true;
}

ImageLoadInfo loadImage(AXI32_Slave &bus, const string &path,
                        bool rawAddrGiven, u32 rawAddr)
{
    ImageLoadInfo info;
    info.ok = false;
    info.entry = 0;

    vector<u8> buf;
    if (!readFile(path, buf)) {
        info.error = "cannot read " + path;
        return info;
    }
    if (buf.empty()) {
        info.error = path + " is empty";
        return info;
    }

    bool elf = isElfImage(buf);
    bool uimage = isUImage(buf);

    // An explicit @<addr> stages the whole file verbatim (ELF/uImage header
    // included) at that physical address, ready for a bootloader to consume.
    if (rawAddrGiven) {
        info.format = elf ? "ELF32 staged" : (uimage ? "uImage staged" : "raw");
        info.ok = stageAt(bus, info, buf, rawAddr);
        return info;
    }

    // No address: place the image where the link/header says it belongs.
    if (elf) {
        info.format = "ELF32";
        info.ok = loadElf(bus, buf, info);
        return info;
    }
    if (uimage) {
        info.format = "uImage";
        info.ok = loadUImage(bus, buf, info);
        return info;
    }

    info.format = "raw";
    info.error = "raw binary needs a load address: use <file>@<phys_addr>";
    return info;
}
