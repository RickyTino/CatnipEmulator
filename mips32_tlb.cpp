#include "mips32_tlb.h"

// Page-mask encoding is a run of consecutive 1's (see MangoMIPS32 CP0.v
// "w_mask"): 4K = 0x0000, 16K = 0x0003, 64K = 0x000f, ... 256M = 0xffff.
static u32 popcount(u32 x)
{
    u32 c = 0;
    while (x) {
        x &= x - 1;
        ++c;
    }
    return c;
}

u32 MIPS32_TLBItem::maskBits() const
{
    // Number of low VPN2 bits covered by the page mask.  For every value a
    // program is allowed to write (Vol III 9.10 Table 9.16: 0x0, 0x3, 0xf,
    // 0x3f ... 0xffff) this is just the run of 1's from bit 0.  Loading any
    // other value is UNDEFINED (same section), so a plain popcount is fine
    // here - do not try to replicate TLBU.v's casez fallback for illegal
    // masks.
    return popcount(mask);
}

u32 MIPS32_TLBItem::eob() const
{
    return 12 + maskBits();
}

bool MIPS32_TLBItem::vpn2Match(u32 vaddr) const
{
    // Compare the page-mask-visible part of VPN2 (bits above the masked
    // low bits); the masked low VPN2 bits belong to the page offset and are
    // deliberately ignored, exactly like TLBU.v's masked-XOR comparator.
    u32 p = maskBits();
    u32 a = (vaddr >> 13);
    u32 b = vpn2;
    if (p) {
        a >>= p;
        b >>= p;
    }
    return a == b;
}

MIPS32_TLB::MIPS32_TLB()
{
    reset();
}

void MIPS32_TLB::reset()
{
    for (u32 i = 0; i < MIPS32_TLB_SIZE; ++i)
        items[i] = MIPS32_TLBItem();
}

void MIPS32_TLB::writeEntry(u32 index, u32 pagemask, u32 entryhi,
                            u32 entrylo0, u32 entrylo1)
{
    MIPS32_TLBItem &it = items[index & 0x1F];
    u32 p = popcount((pagemask >> 13) & 0xFFFF);
    u32 vpn2_mask = p ? ((1u << p) - 1) : 0;   // masked low VPN2 bits
    u32 pfn_mask  = p ? ((1u << p) - 1) : 0;   // masked low PFN bits

    it.mask = (pagemask >> 13) & 0xFFFF;
    it.vpn2 = ((entryhi >> 13) & 0x7FFFF) & ~vpn2_mask;
    it.asid = entryhi & 0xFF;
    it.g    = (entrylo0 & 0x1) && (entrylo1 & 0x1);

    it.pfn0 = ((entrylo0 >> 6) & 0xFFFFF) & ~pfn_mask;
    it.c0   = (entrylo0 >> 3) & 0x7;
    it.d0   = (entrylo0 >> 2) & 0x1;
    it.v0   = (entrylo0 >> 1) & 0x1;

    it.pfn1 = ((entrylo1 >> 6) & 0xFFFFF) & ~pfn_mask;
    it.c1   = (entrylo1 >> 3) & 0x7;
    it.d1   = (entrylo1 >> 2) & 0x1;
    it.v1   = (entrylo1 >> 1) & 0x1;
}

bool MIPS32_TLB::translate(u32 vaddr, u32 asid, u32 &pfn, bool &vld,
                           bool &drt, u32 &cat, u32 &eob) const
{
    for (u32 i = 0; i < MIPS32_TLB_SIZE; ++i) {
        const MIPS32_TLBItem &it = items[i];
        if (!it.vpn2Match(vaddr)) continue;
        if (!(it.g || it.asid == asid)) continue;

        bool odd = (vaddr >> it.eob()) & 0x1;
        pfn = odd ? it.pfn1 : it.pfn0;
        vld = odd ? it.v1   : it.v0;
        drt = odd ? it.d1   : it.d0;
        cat = odd ? it.c1   : it.c0;
        eob = it.eob();
        return true;
    }
    return false;
}

int MIPS32_TLB::probe(u32 vpn2, u32 asid) const
{
    for (u32 i = 0; i < MIPS32_TLB_SIZE; ++i) {
        const MIPS32_TLBItem &it = items[i];
        u32 p = it.maskBits();
        u32 a = vpn2;
        u32 b = it.vpn2;
        if (p) {
            a >>= p;
            b >>= p;
        }
        if (a != b) continue;
        if (it.g || it.asid == asid) return (int)i;
    }
    return -1;
}
