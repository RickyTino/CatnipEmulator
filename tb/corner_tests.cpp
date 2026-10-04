// Corner tests: guest-assembly regression tests for core behaviour that the
// other test suites do not reach.
//
// Why this exists: tb/nscscc_tests compares the core against the golden trace,
// which says whether the emulator behaves like the MangoMIPS32 RTL - not
// whether it behaves correctly - and the trace largely misses exception paths.
// Every case here is something that was once wrong (DIV by zero taking the
// emulator down, a nullified likely-branch slot mislabelling EPC/Cause.BD, a
// CP1 encoding reported as RI rather than CpU, an undecoded access reported
// nowhere), so it stays fixed.
//
// How a case works: the guest program is a raw image assembled from
// tb/soft/corner/*.s (see that directory's Makefile).  It writes its result
// words to RESULT_BASE in RAM and then spins, so the host runs a fixed cycle
// budget - a completion test would be pointless - and reads the result block
// back through the bus.  Nothing goes through the UART, which keeps the suite
// clear of the console's terminal handling and of stdout capture.
//
// Each case gets a fresh machine: MIPS32_Core::reset() is private, and CP0
// state (EXL, EPC) would otherwise leak out of the first exception case into
// every later one.  The address map mirrors CatnipSoC without the UART (1MB of
// RAM at 0, a 1MB "bootrom" holding the reset vector at 0x1FC00000), so an
// address outside both really is undecoded.
//
// Adding a case: write tb/soft/corner/<name>.s, run "make -C tb soft-corner",
// then add a row to CASES with the words the guest stores and, if it should
// warn about undecoded accesses, how many lines that is.
//
//   ./tb/corner_tests                 run every case
//   ./tb/corner_tests <substring>     run the cases whose name contains it
//
// Exit status is 0 only when every selected case passes.
#include "common_defs.h"
#include "axi.h"
#include "mips32_core.h"
#include "image_loader.h"
#include <sstream>

namespace {

// Address map and result block (see the header note).
const u32 RAM_BASE    = 0x00000000;
const u32 RAM_WIDTH   = 20;                    // 1MB, as in tb/nscscc_tests
const u32 BOOT_BASE   = 0x1FC00000;            // reset-vector window
const u32 BOOT_WIDTH  = 20;                    // 1MB
const u32 ENTRY       = 0xBFC00000;            // KSEG1 view of BOOT_BASE
const u32 VECTOR_ADDR = 0x1FC00380;            // BOOT_BASE + 0x380
const u32 RESULT_BASE = 0x100;
const u32 RESULT_WORDS = 2;
const u32 CYCLES      = 4096;                  // the guests end in a spin

// Paths are relative to the project root, like the other harnesses'.
const char *CODE_DIR   = "tb/soft/corner/";
const char *VECTOR_IMG = "tb/soft/corner/vector.bin";

// One machine per case (see the header note about CP0 state leaking).
// Initialization order follows the member declaration order.
struct Machine {
    AXI32_Interconnect bus;
    MIPS32_Core cpu;
    AXI32_RAM ram;
    AXI32_RAM bootrom;

    Machine() : bus(0, 0),
                cpu(&bus),
                ram(RAM_BASE, 1u << RAM_WIDTH, RAM_WIDTH),
                bootrom(BOOT_BASE, 1u << BOOT_WIDTH, BOOT_WIDTH)
    {
        bus.addSlave(&ram);
        bus.addSlave(&bootrom);
    }
};

struct Case {
    const char *name;
    const char *image;              // file name under CODE_DIR
    u32  words;                     // result words the guest stores
    u32  expect[RESULT_WORDS];
    u32  warnLines;                 // expected undecoded-access lines on stderr
    bool warnSilenced;              // expect the "not reported" notice as well
};

// Expected Cause values are the whole register: this machine has no interrupt
// sources (no UART, Compare = 0), so only BD, CE and ExcCode can be non-zero.
//   ADEL = 4 -> 0x10, RI = 10 -> 0x28, BD is bit 31.
const Case CASES[] = {
    { "div_zero",      "div_zero.bin",      2, { 0x00000000, 0x00000000 }, 0, false },
    { "div_min",       "div_min.bin",       2, { 0x00000000, 0x80000000 }, 0, false },
    { "likely_null",   "likely_null.bin",   2, { 0xBFC00008, 0x00000010 }, 0, false },
    { "likely_slot",   "likely_slot.bin",   2, { 0xBFC00000, 0x80000010 }, 0, false },
    { "cop1_ri",       "cop1_ri.bin",       2, { 0xBFC00000, 0x00000028 }, 0, false },
    { "unmapped_read", "unmapped_read.bin", 1, { 0x00000000, 0x00000000 }, 8, true  },
};

// Stage a raw image at a physical address through the same loader cemu uses.
static bool stageImage(AXI32_Slave &bus, const char *path, u32 phys)
{
    ImageLoadInfo info = loadImage(bus, path, true, phys);
    if (!info.ok) {
        cerr << EMU_TAG << "error: " << info.error << endl;
        return false;
    }
    return true;
}

static u32 countOccurrences(const string &text, const string &needle)
{
    u32 n = 0;
    for (size_t at = text.find(needle); at != string::npos;
         at = text.find(needle, at + needle.size()))
        ++n;
    return n;
}

static void report(const Case &c, const string &what)
{
    cout << "corner: " << c.name;
    for (size_t pad = strlen(c.name); pad < 16; ++pad)
        cout << ' ';
    cout << what << endl;
}

// Run one case; return false (having said why) on any mismatch.
static bool runCase(const Case &c)
{
    Machine m;

    string image = string(CODE_DIR) + c.image;
    if (!stageImage(m.bus, image.c_str(), BOOT_BASE) ||
        !stageImage(m.bus, VECTOR_IMG, VECTOR_ADDR))
        return false;

    m.cpu.setEntry(ENTRY);

    // The undecoded-access notice goes to stderr; capture it so it can be
    // asserted instead of mixing with this harness's own output.
    ostringstream captured;
    streambuf *saved = cerr.rdbuf(captured.rdbuf());
    for (u32 i = 0; i < CYCLES; ++i)
        m.cpu.cycle();
    cerr.rdbuf(saved);

    bool ok = true;
    ostringstream detail;

    for (u32 i = 0; i < c.words; ++i) {
        u32 got = m.bus.read(RESULT_BASE + 4 * i, 4);
        if (got != c.expect[i]) {
            detail << "        result[" << i << "] expected 0x" << hex
                   << c.expect[i] << ", got 0x" << got << dec << endl;
            ok = false;
        }
    }

    u32 warned   = countOccurrences(captured.str(), "unmapped address");
    u32 silenced = countOccurrences(captured.str(), "not reported");
    if (warned != c.warnLines) {
        detail << "        expected " << c.warnLines
               << " undecoded-access warnings, got " << warned << endl;
        ok = false;
    }
    if ((silenced > 0) != c.warnSilenced) {
        detail << "        suppression notice"
               << (c.warnSilenced ? " missing" : " unexpected") << endl;
        ok = false;
    }

    report(c, ok ? "PASS" : "FAIL");
    cout << detail.str();
    return ok;
}

}  // namespace

int main(int argc, char *argv[])
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    if (argc > 2) {
        cerr << "usage: " << argv[0] << " [case-name-substring]" << endl;
        return 1;
    }

    u32 total = 0, failed = 0;
    for (u32 i = 0; i < sizeof(CASES) / sizeof(CASES[0]); ++i) {
        const Case &c = CASES[i];
        if (filter && !strstr(c.name, filter))
            continue;
        ++total;
        if (!runCase(c))
            ++failed;
    }

    if (total == 0) {
        cerr << EMU_TAG << "no case matches '" << filter << "'" << endl;
        return 1;
    }
    cout << "Corner tests " << (failed ? "FAIL" : "PASS") << " ("
         << total - failed << "/" << total << ")." << endl;
    return failed ? 1 : 0;
}
