// cemu - the CatnipEmulator entry point.
//
//   ./cemu                          print this usage
//   ./cemu <image> [<image> ...]    ELF/uImage at its linked address
//   ./cemu <image>@<phys> ...       stage the whole file at <phys>
//   ./cemu <image>... --entry <pc>  override the start PC (virtual)
//
// With @<phys> the file is written verbatim (headers included) at that physical
// address - e.g. pre-staging a uImage for "bootm" in u-boot.  Without it,
// ELF/uImage go where they were linked and a raw binary is rejected.  The start
// PC defaults to the reset vector 0xBFC00000 and is a virtual address, e.g.
//   ./cemu u-boot.bin@0x00200000 --entry 0x80200000
#include "catnipsoc.h"
#include "image_loader.h"
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <vector>

static bool parseAddr(const string &s, u32 &out)
{
    // Standard C conventions: "0x..." hex, leading 0 octal, else decimal.
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(s.c_str(), &end, 0);
    if (end == s.c_str() || *end != '\0' || errno != 0 || v > 0xFFFFFFFFul)
        return false;
    out = (u32)v;
    return true;
}

struct ImageSpec {
    string file;
    bool   addrGiven;
    u32    addr;
};

static string hex8(u32 v)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%08X", v);
    return string(buf);
}

static void usage(const char *prog)
{
    cout << "usage: " << prog << " <image>[@<phys>] [<image>[@<phys>] ...]"
            " [--entry <pc>]" << endl;
    cout << "  ELF/uImage are placed where they were linked; with @<phys> the"
            " whole file is staged verbatim there." << endl;
    cout << "  Start PC defaults to the reset vector 0x"
         << hex8(CATNIPSOC_RESET).substr(2) << "." << endl;
}

// Load every image, then run from the entry (--entry, else the reset vector).
static int run_images(const vector<ImageSpec> &specs,
                      bool entryGiven, u32 entryOverride)
{
    CatnipSoC soc;
    vector<ImageSegment> placed;   // everything loaded so far, for overlap checks

    for (size_t k = 0; k < specs.size(); ++k) {
        const ImageSpec &sp = specs[k];
        ImageLoadInfo info = loadImage(soc.getBus(), sp.file, sp.addrGiven, sp.addr);
        if (!info.ok) {
            cerr << EMU_TAG << "error: " << info.error << endl;
            return 1;
        }

        cout << EMU_TAG << "loaded " << sp.file << " (" << info.format << "):" << endl;
        for (size_t i = 0; i < info.segments.size(); ++i) {
            const ImageSegment &seg = info.segments[i];
            cout << EMU_TAG << "  " << hex8(seg.addr) << " +" << hex8(seg.size)
                 << "  -> " << soc.regionName(seg.addr);

            for (size_t j = 0; j < placed.size(); ++j) {
                if (seg.addr < placed[j].addr + placed[j].size &&
                    placed[j].addr < seg.addr + seg.size) {
                    cout << "  [overlaps " << hex8(placed[j].addr) << "]";
                    break;
                }
            }
            cout << endl;
            placed.push_back(seg);
        }

        // The image's own entry is only meaningful when it was placed where it
        // was linked (no explicit @addr).
        if (!sp.addrGiven && info.format != "raw")
            cout << EMU_TAG << "  image entry " << hex8(info.entry) << endl;
    }

    u32 entry = entryGiven ? entryOverride : CATNIPSOC_RESET;
    cout << EMU_TAG << "start PC " << hex8(entry) << "  -> "
         << soc.regionName(entry & 0x1FFFFFFF)
         << (entryGiven ? "" : "  (reset vector)") << endl;
    cout << EMU_TAG << "running (Ctrl-A x to quit)..." << endl;

    soc.getCpu().setEntry(entry);
    soc.getUart().openConsole();
    while (true)
        soc.step();
    return 0;
}

int main(int argc, char *argv[])
{
    vector<ImageSpec> specs;
    bool entryGiven = false;
    u32 entryOverride = 0;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--entry") {
            if (i + 1 >= argc || !parseAddr(argv[++i], entryOverride)) {
                cerr << EMU_TAG << "error: --entry needs a numeric address" << endl;
                return 1;
            }
            entryGiven = true;
        } else if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
            return 0;
        } else if (!arg.empty() && arg[0] == '-') {
            cerr << EMU_TAG << "error: unknown option '" << arg << "'" << endl;
            return 1;
        } else {
            ImageSpec sp;
            sp.addrGiven = false;
            sp.addr = 0;
            size_t at = arg.find('@');
            if (at != string::npos) {
                sp.file = arg.substr(0, at);
                if (!parseAddr(arg.substr(at + 1), sp.addr)) {
                    cerr << EMU_TAG << "error: bad address in '" << arg << "'" << endl;
                    return 1;
                }
                sp.addrGiven = true;
            } else {
                sp.file = arg;
            }
            specs.push_back(sp);
        }
    }

    if (specs.empty()) {
        usage(argv[0]);
        return 1;
    }
    return run_images(specs, entryGiven, entryOverride);
}
