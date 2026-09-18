# CatnipEmulator - run targets (boot the SoC, run the tests).
#
# Build first with the normal Makefile, then run from the project root:
#
#   make                     # build ./cemu, ./nscscc_tests, ./uart_test
#   make -f run.mk linux     # u-boot + kernel
#
# Targets:
#   make -f run.mk uboot         u-boot alone (interactive)
#   make -f run.mk linux         u-boot + kernel; type "bootm 0x81000000"
#   make -f run.mk linux-auto    hands-free Linux boot (auto-sends "bootm")
#   make -f run.mk linux-direct  boot the kernel directly, no u-boot
#   make -f run.mk nscscc-func   NSCSCC functional test (golden trace)
#   make -f run.mk nscscc-perf   NSCSCC performance benchmark
#   make -f run.mk nscscc-tlb    NSCSCC TLB test
#   make -f run.mk uart-test     uart16550 self-test
#
# Any value can be overridden on the command line, e.g.
#   make -f run.mk linux UIMAGE=/path/to/other/uImage.bin

EMU      ?= ./cemu
TESTS    ?= ./nscscc_tests
UARTSELF ?= ./uart_test

# Default images (paths relative to the project root).
BOARD    ?= ../u-boot-catnipsoc
KERNEL   ?= ../linux-5.6.14-catnipsoc
UBOOT    ?= $(BOARD)/u-boot.bin
UIMAGE   ?= $(KERNEL)/arch/mips/boot/uImage.bin
VMLINUX  ?= $(KERNEL)/vmlinux

# Physical load addresses, the start PC, and the u-boot "bootm" argument.
UBOOT_ADDR   ?= 0x00200000
UIMAGE_ADDR  ?= 0x01000000
ENTRY        ?= 0x80200000
BOOTM_ADDR   ?= 0x81000000
DELAY        ?= 3

# Direct kernel boot (no u-boot): the loader places the vmlinux segments at
# their linked addresses and we jump to the kernel's ELF entry point, read from
# the ELF itself so it stays correct if the kernel is relinked.  Set
# KENTRY=0x... to skip the readelf probe (or if readelf is unavailable).
READELF      ?= readelf
KENTRY       ?= $(shell $(READELF) -h $(VMLINUX) 2>/dev/null | awk '/Entry point/{print $$4}')

IMAGES = $(UBOOT)@$(UBOOT_ADDR) $(UIMAGE)@$(UIMAGE_ADDR)

.PHONY: help uboot linux linux-auto linux-direct \
        nscscc-func nscscc-perf nscscc-tlb uart-test

help:
	@echo "make -f run.mk uboot         run u-boot (interactive)"
	@echo "make -f run.mk linux         u-boot + kernel; at the prompt: bootm $(BOOTM_ADDR)"
	@echo "make -f run.mk linux-auto    hands-free Linux boot (via u-boot)"
	@echo "make -f run.mk linux-direct  boot the kernel directly, no u-boot"
	@echo "make -f run.mk nscscc-func   NSCSCC functional test (golden trace)"
	@echo "make -f run.mk nscscc-perf   NSCSCC performance benchmark"
	@echo "make -f run.mk nscscc-tlb    NSCSCC TLB test"
	@echo "make -f run.mk uart-test     uart16550 self-test"

# --- SoC boots --------------------------------------------------------------

# u-boot alone, on the interactive console.
uboot:
	$(EMU) $(UBOOT)@$(UBOOT_ADDR) --entry $(ENTRY)

# u-boot + kernel staged, then interactive.
linux:
	@echo "== at the u-boot prompt, type:  bootm $(BOOTM_ADDR)"
	$(EMU) $(IMAGES) --entry $(ENTRY)

# Hands-free: "bootm" is sent automatically after $(DELAY) seconds.  stdin is a
# pipe here, so the guest console is line-buffered rather than raw; Ctrl-A x
# still quits.
linux-auto:
	( sleep $(DELAY); printf 'bootm $(BOOTM_ADDR)\r'; sleep 100000 ) \
	  | $(EMU) $(IMAGES) --entry $(ENTRY)

# Boot the kernel directly, without u-boot.
linux-direct:
	@test -n "$(KENTRY)" || { \
		echo "run.mk: cannot read the kernel entry from $(VMLINUX); set KENTRY=0x..."; \
		exit 1; }
	$(EMU) $(VMLINUX) --entry $(KENTRY)

# --- NSCSCC tests -----------------------------------------------------------

nscscc-func:
	$(TESTS) func

nscscc-perf:
	@read -p "Switch value: " sw; printf '%s\n' "$$sw" | $(TESTS) perf

nscscc-tlb:
	$(TESTS) tlb

# --- Peripheral self-tests --------------------------------------------------

uart-test:
	$(UARTSELF)
