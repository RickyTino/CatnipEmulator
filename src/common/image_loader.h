#ifndef __IMAGE_LOADER_H__
#define __IMAGE_LOADER_H__

#include "common_defs.h"
#include "axi.h"

// One region placed by the loader.
struct ImageSegment {
    u32 addr;   // physical address
    u32 size;   // bytes written
};

struct ImageLoadInfo {
    bool   ok;
    string error;
    string format;               // "ELF32", "uImage", "raw"
    u32    entry;                // start PC, as stored in the image / given addr
    vector<ImageSegment> segments;
};

// Load an image into the address space behind `bus`.  The bytes are written at
// the *physical* address derived from the image (MIPS vaddr & 0x1FFFFFFF), so
// the interconnect picks the target region (memory / bootrom / ...) by address.
//
// Recognised containers (by magic, not extension):
//   - ELF32 little-endian MIPS : each PT_LOAD at p_paddr (fallback p_vaddr);
//                                both are KSEG addresses, masked to physical
//   - U-Boot legacy uImage     : payload at ih_load (likewise KSEG)
//   - anything else            : raw binary
//
// rawAddrGiven (an explicit @<addr> on the command line):
//   the whole file is staged verbatim, headers included, at that *physical*
//   address - ready for a bootloader (u-boot bootm / bootelf) to consume.
// Otherwise:
//   an ELF/uImage is placed where it was linked; a raw binary is rejected
//   because it carries no address.
ImageLoadInfo loadImage(AXI32_Slave &bus, const string &path,
                        bool rawAddrGiven, u32 rawAddr);

#endif
