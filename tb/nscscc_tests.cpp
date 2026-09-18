// NSCSCC contest test harness.
//
//   ./nscscc_tests            -> run_perf (default)
//   ./nscscc_tests perf       -> run_perf (performance benchmarks)
//   ./nscscc_tests func       -> run_func (functional tests vs golden trace)
//   ./nscscc_tests tlb        -> run_tlb  (TLB functional test)
//
// The test programs are loaded straight from their ELF build output under
// tb/soft/ (see the NSCSCC_*_ELF paths), so there is no separate hex dump to
// keep in sync.
#include "nscscc_tests.h"
#include "image_loader.h"
#include <bitset>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#ifndef OS_IS_WINDOWS
    #include <unistd.h>
#endif

NscsccConfreg::NscsccConfreg() :
AXI32_Slave(0x1FAF0000, 0x10000)
{
    // Everything starts defined: the tests poll some of these (LED_RG0/LED_RG1
    // are the TLB suite's completion signal, and SIMU_FLAG must read non-zero
    // so the guest skips its simulated 1-second delays).
    for (int i = 0; i < 8; ++i)
        cr[i] = 0;
    led = 0;
    led_rg0 = 0;
    led_rg1 = 0;
    digit = 0;
    swt = 0;
    swinter = 0x0000aaaa;
    timer = 0;
    iosimu = 0;
    uart = 0;
    simflag = 0xFFFFFFFF;
    opentrace = false;
    // No guest code writes NUM_MONITOR: it is a host-side switch that enables
    // reporting writes to NUM as "Functional Test Point PASS".  Default on.
    monitor = 1;
    uart_end = false;
}

void NscsccConfreg::cycle()
{
    timer = timer + 1;
}

void NscsccConfreg::write_uart(u32 data)
{
    if(!uart_end){
        uart = data;
        char ch = data;
        if((u8)ch == 0xFF) {
            uart_end = true;
        }
        else {
            #ifdef OS_IS_WINDOWS
                SetConsoleTextAttribute(
                    GetStdHandle(STD_OUTPUT_HANDLE),
                    BACKGROUND_INTENSITY
                );
                cout << ch;
                SetConsoleTextAttribute(
                    GetStdHandle(STD_OUTPUT_HANDLE),
                    FOREGROUND_INTENSITY
                );
            #else
                // Only colorize when writing to a terminal, so redirected
                // output stays free of escape sequences.
                if (isatty(fileno(stdout)))
                    cout << "\033[0;34m" << ch << "\033[0m";
                else
                    cout << ch;
            #endif
        }
    }
}

u32 NscsccConfreg::readw(u32 addr)
{
    u32 res;
    switch (addr & 0xFFFF){
        case CR0_ADDR: res = cr[0]; break;
        case CR1_ADDR: res = cr[1]; break;
        case CR2_ADDR: res = cr[2]; break;
        case CR3_ADDR: res = cr[3]; break;
        case CR4_ADDR: res = cr[4]; break;
        case CR5_ADDR: res = cr[5]; break;
        case CR6_ADDR: res = cr[6]; break;
        case CR7_ADDR: res = cr[7]; break;

        case LED_ADDR:          res = led;     break;
        case LED_RG0_ADDR:      res = led_rg0; break;
        case LED_RG1_ADDR:      res = led_rg1; break;
        case NUM_ADDR:          res = digit;   break;
        case SWITCH_ADDR:       res = swt;     break;
        // case BTN_KEY_ADDR: res = 0;       break; //button not yet implemented
        // case BTN_STEP_ADDR: res = 0;      break;
        case SW_INTER_ADDR:     res = swinter; break;
        case TIMER_ADDR:        res = timer;   break;
        case IO_SIMU_ADDR:      res = iosimu;  break;
        case VIRTUAL_UART_ADDR: res = uart;      break;
        case SIMU_FLAG_ADDR:    res = simflag;   break;
        case OPEN_TRACE_ADDR:   res = opentrace; break;
        case NUM_MONITOR_ADDR:  res = monitor;   break;

        default: res = 0; break;
    }
    return res;
}

// `mask` is part of the AXI32_Slave interface but is not used here: the
// confreg is word-addressable, so byte-enables are ignored.
void NscsccConfreg::writew(u32 data, u32 addr, u32 /*mask*/)
{
    switch (addr & 0xFFFF){
        case CR0_ADDR: cr[0] = data; break;
        case CR1_ADDR: cr[1] = data; break;
        case CR2_ADDR: cr[2] = data; break;
        case CR3_ADDR: cr[3] = data; break;
        case CR4_ADDR: cr[4] = data; break;
        case CR5_ADDR: cr[5] = data; break;
        case CR6_ADDR: cr[6] = data; break;
        case CR7_ADDR: cr[7] = data; break;

        case LED_ADDR:
            led = data;
            cout << "Confreg > led: " << bitset<16>(led) << endl;
            break;

        case LED_RG0_ADDR:
            led_rg0 = data;
            cout << "Confreg > led_rg0: " << hex << led_rg0 << endl;
            break;

        case LED_RG1_ADDR:
            led_rg1 = data;
            cout << "Confreg > led_rg1: " << hex << led_rg1 << endl;
            break;

        case NUM_ADDR:
            digit = data;
            cout << "Confreg > num: " << hex << digit << endl;
            if(monitor)
                cout << "Number "<< dec << (digit >> 24) << " Functional Test Point PASS!!!" << endl;
            break;

        case TIMER_ADDR:
            timer = data;
            cout << "Confreg > set timer = " << hex << timer << endl;
            break;
        case IO_SIMU_ADDR:      iosimu    = bitConcat(data, 16, bitPart(data, 31, 16)); break;
        case VIRTUAL_UART_ADDR: write_uart(data); break;
        case SIMU_FLAG_ADDR:    simflag   = data; break;
        case OPEN_TRACE_ADDR:   opentrace = data; break;
        case NUM_MONITOR_ADDR:  monitor   = data; break;

        default: break;
    }
}

// Initialization order follows the member declaration order in nscscc_tests.h
// (bus, tracer, cpu, sram, confreg) so that -Wreorder stays quiet.
NscsccTests::NscsccTests() :
bus(0, 0),
tracer(NSCSCC_GOLDEN_TRACE),
cpu(&bus, &tracer),
sram(0, 0, SRAM_WIDTH),
confreg()
{
    bus.addSlave(&confreg);
    bus.addSlave(&sram);
}

void NscsccTests::loadTestImage(const char *path)
{
    // Load through the bus: the SRAM masks addresses to its 1MB window, so the
    // ELF's KSEG link addresses land at the right SRAM offsets.
    ImageLoadInfo info = loadImage(bus, path, false, 0);
    if (!info.ok) {
        cerr << EMU_TAG << "error: " << info.error << endl;
        exit(1);
    }

    cout << EMU_TAG << "loaded " << path << " (" << info.format << "):" << endl;
    for (size_t i = 0; i < info.segments.size(); ++i) {
        cout << EMU_TAG << "  0x" << hex << info.segments[i].addr
             << " +0x" << info.segments[i].size
             << "  -> sram" << endl;
    }
    cout << EMU_TAG << "start PC 0x" << hex << info.entry << dec << endl;

    cpu.setEntry(info.entry);
}

void NscsccTests::run_func()
{
    // Functional test vs the golden trace.  The program reports its result
    // through the LEDs at test_end and then spins forever in start.S's
    // test_finish, writing 0xFF to the virtual UART.  Completion is therefore
    // signalled in one of two ways, whichever comes first:
    //   - 0xFF written to the virtual UART (test_finish's terminator), or
    //   - the LED write in test_end.
    // Led values over the run (the start marker looks like a mix of both
    // outcomes, so only these two exact combinations mean "done"):
    //   start        : LED_RG1 = 2, LED_RG0 = 1
    //   all passed   : LED_RG1 = 1, LED_RG0 = 1   <- done, PASS
    //   a point fails: LED_RG1 = 2, LED_RG0 = 2   <- done, FAIL
    loadTestImage(NSCSCC_FUNC_ELF);
    confreg.swt = 0;

    bool pass = false;
    while (1) {
        tracer.trigger(confreg.opentrace);
        cpu.cycle();
        confreg.cycle();

        if (confreg.led_rg1 == 1) {      // test_end, all points passed
            pass = true;
            break;
        }
        if (confreg.led_rg0 == 2) {      // test_end, a point failed
            pass = false;
            break;
        }
        if (confreg.uart_end) break;     // test_finish terminator
    }

    if (pass)
        cout << "Functional test PASS." << endl;
    else
        cout << "Functional test FAIL (led_rg0 = " << hex << confreg.led_rg0
             << ", led_rg1 = " << hex << confreg.led_rg1
             << ", num = " << confreg.digit << dec << ")." << endl;
}

void NscsccTests::run_perf()
{
    loadTestImage(NSCSCC_PERF_ELF);
    tracer.trigger(false);
    // Switch value: decimal, or 0x-prefixed hex.  The switch register is
    // active-low, so its value is the bitwise complement of the input.
    cout << "Switch value: ";
    string sw_str;
    if (!(cin >> sw_str)) {
        cerr << "error: no switch value given (stdin is empty/closed)" << endl;
        exit(1);
    }
    char *endp = NULL;
    unsigned long sw = strtoul(sw_str.c_str(), &endp, 0);
    if (endp == sw_str.c_str() || *endp != '\0' || sw > 0xFFFFFFFFul) {
        cerr << "error: bad switch value '" << sw_str << "'" << endl;
        exit(1);
    }
    confreg.swt = ~(u32)sw;
    clock_t start, end;
    start = clock();
    while (1) {
        //tracer.trigger(confreg.opentrace);
        cpu.cycle();
        confreg.cycle();
        if(confreg.uart_end) break;
    }
    end = clock();
    cout << dec;
    cout << "Execution time (clock): " << (double)(end - start)/CLOCKS_PER_SEC << "s" << endl;
    cout << "Clocks per second: " << CLOCKS_PER_SEC << endl;
}

void NscsccTests::run_tlb()
{
    // TLB functional test (ported from MangoMIPS32 Testbench/TLB_Test).
    // Completion is signalled in one of two ways, whichever comes first:
    //   - 0xFF written to the virtual UART (tb/legacy/tlb_test style), or
    //   - the LED write in start.S's test_end, then it spins in test_finish.
    // Led values over the run (careful - the start marker looks like a mix of
    // both outcomes, so only these two exact combinations mean "done"):
    //   start        : LED_RG1 = 2, LED_RG0 = 1
    //   all passed   : LED_RG1 = 1, LED_RG0 = 1   <- done, PASS
    //   a point fails: LED_RG1 = 2, LED_RG0 = 2   <- done, FAIL
    loadTestImage(NSCSCC_TLB_ELF);
    tracer.trigger(false);
    cout << "TLB test begin." << endl;

    bool pass = false;
    while (1) {
        cpu.cycle();
        confreg.cycle();

        if (confreg.uart_end) {
            pass = true;                 // terminator received
            break;
        }
        if (confreg.led_rg1 == 1) {      // test_end, all points passed
            pass = true;
            break;
        }
        if (confreg.led_rg0 == 2) {      // test_end, a point failed
            pass = false;
            break;
        }
    }

    if (pass)
        cout << "TLB test PASS." << endl;
    else
        cout << "TLB test FAIL (led_rg0 = " << hex << confreg.led_rg0
             << ", led_rg1 = " << confreg.led_rg1 << dec << ")." << endl;
}

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

    NscsccTests tests;

    if (is_func)
        tests.run_func();
    else if (is_tlb)
        tests.run_tlb();
    else
        tests.run_perf();

    return 0;
}
