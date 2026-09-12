#include "soc_lite.h"
#include <cstdlib>
#include <cstdio>
#include <string>
#ifndef OS_IS_WINDOWS
    #include <unistd.h>
#endif

SoCLite_Confreg::SoCLite_Confreg() :
AXI32_Slave(0x1FAF0000, 0x10000)
{
    timer = 0;
    simflag = 0xFFFFFFFF;
    swinter = 0x0000aaaa;
    uart_end = false;
    opentrace = 0;
}

void SoCLite_Confreg::cycle()
{
    timer = timer + 1;
}

void SoCLite_Confreg::write_uart(u32 data)
{
    if(!uart_end){
        uart = data;
        char ch = data;
        if((u8)ch == 0xFF) {
            uart_end = true;
            //cout << "uart end. " << endl;
            //getch();
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

// u8 SoCLite_Confreg::readb(u32 addr)
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

// void SoCLite_Confreg::writeb(u8 data, u32 addr)
// {
//     u32 word_addr = (addr >> 2) % ram.size();
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

u32 SoCLite_Confreg::readw(u32 addr)
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
void SoCLite_Confreg::writew(u32 data, u32 addr, u32 /*mask*/)
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
            cout << "Confreg > led_rg0: " << hex << led_rg1 << endl;
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

// SoCLite_RAM::SoCLite_RAM() :
// AXI32_RAM(0, 0, SRAM_SIZE)
// {

// }

// Initialization order follows the member declaration order in soc_lite.h
// (bus, tracer, cpu, sram) so that -Wreorder stays quiet.
SoCLite::SoCLite() :
bus(0, 0),
tracer("testbench/golden_trace.txt"),
cpu(&bus, &tracer),
sram(0, 0, SRAM_WIDTH)
{
    bus.addSlave(&confreg);
    bus.addSlave(&sram);
}

void SoCLite::run_func()
{
    sram.loadHex("testbench/func_ram.txt", 0);
    //tracer.trigger(false);
    confreg.swt = 0;
    while (1) {
        tracer.trigger(confreg.opentrace);
        cpu.cycle();
        confreg.cycle();
    }
}

void SoCLite::run_perf()
{
    sram.loadHex("testbench/perf_ram.txt", 0);
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

void SoCLite::run_tlb()
{
    // TLB functional test (ported from MangoMIPS32 Testbench/TLB_Test).
    // The guest writes 0xFF to the virtual UART to signal successful
    // completion; any unexpected exception hangs the guest instead.
    sram.loadHex("testbench/tlb_ram.txt", 0);
    tracer.trigger(false);
    cout << "TLB test begin." << endl;
    while (1) {
        cpu.cycle();
        confreg.cycle();
        if (confreg.uart_end) break;
    }
    cout << "TLB test PASS (terminator received)." << endl;
}
