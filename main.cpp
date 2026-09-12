// CatnipEmulator: run mode selector.
//   ./soc_lite            -> run_perf (default, contest demo behavior)
//   ./soc_lite perf       -> run_perf
//   ./soc_lite func       -> run_func (functional tests vs golden trace)
//   ./soc_lite tlb        -> run_tlb  (TLB functional test)
#include "soc_lite.h"
#include <cstring>

static void usage(const char *prog)
{
    cerr << "usage: " << prog << " [perf|func|tlb]" << endl;
}

int main(int argc, char *argv[])
{
    const char *mode = (argc > 1) ? argv[1] : "perf";

    bool is_perf = strcmp(mode, "perf") == 0;
    bool is_func = strcmp(mode, "func") == 0;
    bool is_tlb  = strcmp(mode, "tlb")  == 0;
    if (!is_perf && !is_func && !is_tlb) {
        usage(argv[0]);
        return 1;
    }

    SoCLite soc;

    if (is_func)
        soc.run_func();
    else if (is_tlb)
        soc.run_tlb();
    else
        soc.run_perf();

    return 0;
}
