# CatnipEmulator
#
# Build:
#   make                 build ./cemu, tb/nscscc_tests, tb/uart_test
#   make cemu            the CatnipSoC emulator (loads and runs images)
#   make nscscc_tests    NSCSCC contest test harness (func/perf/tlb)
#   make uart_test       standalone uart16550 self-test
#   make clean
#   make OPT=-O0         build without optimization (for debugging)
#   make OS_IS_WINDOWS=1 build the Windows console-colour path
#
# Run (the tests are run from tb/ - see tb/Makefile):
#   make run             u-boot + kernel; at the prompt: bootm 0x81000000
#   make run_linux       boot the kernel directly, no u-boot
#   make help            list the run targets
#
# Any of the run variables can be overridden, e.g.
#   make run UIMAGE=/path/to/other/uImage.bin

CPP      = g++
OPT     ?= -O2
CXXFLAGS = -g3 $(OPT) -std=c++11 -MMD -MP -Isrc -Isrc/common -Isrc/bus \
           -Isrc/cpu -Isrc/dev -Isrc/soc -Itb
LDFLAGS  = -static-libgcc
BUILD    = build

# Set OS_IS_WINDOWS=1 to build the Windows console-colour path.
ifdef OS_IS_WINDOWS
CXXFLAGS += -DOS_IS_WINDOWS
endif

# ---------------------------------------------------------------------------
# Build.  Sources live in src/*/ and tb/ (found through VPATH, so a plain
# "%.o: %.cpp" rule works); objects land in $(BUILD).  cemu goes to the project
# root, the test binaries next to the assets they load.
# ---------------------------------------------------------------------------

VPATH = src src/common src/bus src/cpu src/dev src/soc tb
objs  = $(addprefix $(BUILD)/,$(addsuffix .o,$(1)))

CEMU = cemu
NSC  = tb/nscscc_tests
UART = tb/uart_test

CEMU_OBJ = $(call objs,main catnipsoc image_loader axi mips32_core \
                       mips32_cp0 mips32_tlb mips32_tracer uart16550)
NSC_OBJ  = $(call objs,nscscc_tests image_loader axi mips32_core \
                       mips32_cp0 mips32_tlb mips32_tracer)
UART_OBJ = $(call objs,uart_test uart16550 axi)

.PHONY: all clean nscscc_tests uart_test

all: $(CEMU) $(NSC) $(UART)

# Short aliases for the two binaries that live in tb/.  ("cemu" is a file
# target at the root already, so "make cemu" needs no alias - and it must not
# be listed as phony, or its file target would always look out of date.)
nscscc_tests: $(NSC)
	@:
uart_test: $(UART)
	@:

$(CEMU): $(CEMU_OBJ)
	$(CPP) $^ -o $@ $(LDFLAGS)

$(NSC): $(NSC_OBJ)
	$(CPP) $^ -o $@ $(LDFLAGS)

$(UART): $(UART_OBJ)
	$(CPP) $^ -o $@ $(LDFLAGS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(BUILD)
	$(CPP) -c $< -o $@ $(CXXFLAGS)

clean:
	$(RM) -r $(BUILD) $(CEMU) $(NSC) $(UART)

# Auto-generated header dependencies (-MMD -MP).  Harmless if not present yet.
-include $(CEMU_OBJ:.o=.d) $(NSC_OBJ:.o=.d) $(UART_OBJ:.o=.d)

# ---------------------------------------------------------------------------
# Run: SoC boots.  Default images are the ones in this workspace; paths are
# relative to the project root.
# ---------------------------------------------------------------------------

EMU      ?= ./cemu

BOARD    ?= ../u-boot-catnipsoc
KERNEL   ?= ../linux-5.6.14-catnipsoc
UBOOT    ?= $(BOARD)/u-boot.bin
UIMAGE   ?= $(KERNEL)/arch/mips/boot/uImage.bin
VMLINUX  ?= $(KERNEL)/vmlinux

# u-boot's TEXT_BASE (0x80200000) as a physical address, where to stage the
# uImage clear of it, the start PC, and the KSEG0 view used for "bootm".
UBOOT_ADDR   ?= 0x00200000
UIMAGE_ADDR  ?= 0x01000000
ENTRY        ?= 0x80200000
BOOTM_ADDR   ?= 0x81000000

# run_linux jumps straight to the kernel entry, read from the ELF so it stays
# correct if the kernel is relinked.
KENTRY       ?= $(shell readelf -h $(VMLINUX) 2>/dev/null | awk '/Entry point/{print $$4}')

.PHONY: help run run_linux

help:
	@echo "make run         u-boot + kernel; at the prompt: bootm $(BOOTM_ADDR)"
	@echo "make run_linux   boot the kernel directly, no u-boot"
	@echo "(tests: make -C tb help)"

# u-boot + kernel, staged in RAM, then interactive.
run:
	@echo "== at the u-boot prompt, type:  bootm $(BOOTM_ADDR)"
	$(EMU) $(UBOOT)@$(UBOOT_ADDR) $(UIMAGE)@$(UIMAGE_ADDR) --entry $(ENTRY)

# Boot the kernel directly, without u-boot.
run_linux:
	@test -n "$(KENTRY)" || { \
		echo "cannot read the kernel entry from $(VMLINUX); set KENTRY=0x..."; \
		exit 1; }
	$(EMU) $(VMLINUX) --entry $(KENTRY)
