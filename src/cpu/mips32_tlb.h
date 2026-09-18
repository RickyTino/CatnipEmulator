#ifndef __MIPS32_TLB_H__
#define __MIPS32_TLB_H__

#include "common_defs.h"
#include "bit_utils.h"

// Behavior reference: MangoMIPS32 Src/Core/TLBU.v & CP0.v (Defines.v).
// 32-entry, fully-associative TLB for MIPS32 Release 1.

#define MIPS32_TLB_SIZE     32

// One TLB entry.  Field meanings (see Defines.v `TLB_Itm`):
//   vpn2 : EntryHi[31:13], page-mask masked on write
//   asid : EntryHi[7:0]
//   mask : PageMask[28:13] contents; for the legal values a run of
//          bit-pairs, i.e. 0, 0x3, 0xf, 0x3f ... 0xffff (Vol III 9.10)
//   g    : EntryLo0.G & EntryLo1.G  (entry global -> ASID ignored on match)
//   pfn0/1, v0/1, d0/1, c0/1 : even/odd half-page fields from EntryLo0/1
class MIPS32_TLBItem {
public:
    u32  vpn2;
    u32  asid;
    u32  mask;
    bool g;
    u32  pfn0, pfn1;
    bool v0, v1;
    bool d0, d1;
    u32  c0, c1;

    MIPS32_TLBItem() :
        vpn2(0), asid(0), mask(0), g(false),
        pfn0(0), pfn1(0), v0(false), v1(false),
        d0(false), d1(false), c0(0), c1(0) {}

    // Number of low VPN2 bits covered by the page mask (popcount of mask).
    u32 maskBits() const;
    // Even/odd boundary bit index: 12 + maskBits.  Half page size = 2^eob.
    u32 eob() const;
    // Address-part match of a virtual address against this entry.
    bool vpn2Match(u32 vaddr) const;
};

// Fully-associative TLB array, mirroring the search/read/write behavior of
// TLBU.v.  Compare/lookup semantics:
//   match  = ((entry.vpn2 ^ vaddr[31:13]) masked by ~page_mask) == 0
//   avail  = entry.g || entry.asid == lookup-asid
//   half   = vaddr[eob] selects pfn1/pfn0, v1/v0, d1/d0, c1/c0
//   paddr  = (pfn << 12) | (vaddr & ((1 << eob) - 1))
class MIPS32_TLB {
private:
    MIPS32_TLBItem items[MIPS32_TLB_SIZE];

public:
    MIPS32_TLB();

    // Invalidate all entries (reset).
    void reset();

    // Read the entry selected by Index (TLBR path).
    const MIPS32_TLBItem &get(u32 index) const { return items[index & 0x1F]; }

    // Program one entry (TLBWI / TLBWR path).  Registers are supplied as raw
    // CP0 word values; the stored fields are masked down exactly as TLBU.v:
    //   vpn2 = EntryHi[31:13] & ~PageMask[31:13]
    //   pfn  = EntryLo[25:6] with the low maskBits() bits cleared
    //   g    = EntryLo0.G & EntryLo1.G
    // TLBU.v stores `EntryLo[PFN] & ~PageMask[31:12]`, which is off by one
    // against its own paddr mux (`{i_pfn[19:2], vaddr[13:0]}` for a 16KB
    // page); clearing the low maskBits() bits is what the datapath needs.
    void writeEntry(u32 index, u32 pagemask, u32 entryhi, u32 entrylo0, u32 entrylo1);

    // Translation search (I/D access).  Finds the lowest-index entry whose
    // masked VPN2 matches vaddr and whose ASID matches (unless global).
    // Returns false when nothing matches (TLB Refill).  On success the fields
    // of the selected half page are returned; V/D validity is reported, not
    // enforced, so the caller can raise TLB Invalid / Modified.
    bool translate(u32 vaddr, u32 asid, u32 &pfn, bool &vld, bool &drt,
                   u32 &cat, u32 &eob) const;

    // TLBP probe: like translate but the target comes from the EntryHi
    // register (vpn2 already carries the [31:13] field position).
    // Returns -1 on miss, otherwise the matched entry index.
    int probe(u32 vpn2, u32 asid) const;

    int size() const { return MIPS32_TLB_SIZE; }
};

#endif
