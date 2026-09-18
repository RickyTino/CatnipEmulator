# Run the NSCSCC test harness and the uart self-test.
#
# The binaries are built here by the top-level Makefile ("make" in the project
# root) and live next to their assets.  They look those assets up - tb/soft/...,
# the golden trace - relative to the PROJECT ROOT, so they are always started
# from there.  That keeps the same command working whether this file is invoked
# as "make -C tb ..." or "cd tb && make ...".
#
#   make -C tb nscscc-func   NSCSCC functional test (compares against the golden trace)
#   make -C tb nscscc-perf   NSCSCC performance benchmark (prompts for a switch value)
#   make -C tb nscscc-tlb    NSCSCC TLB test
#   make -C tb uart-test     uart16550 self-test
#
# nscscc-func never exits by itself (the guest spins once it is done) - Ctrl-C.
#
# "make -C tb ..." makes make announce the directory it entered and left; run
# from inside tb to avoid that, or add --no-print-directory.

# Project root, wherever make was started from.
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))..)

NSCSCC   ?= ./tb/nscscc_tests
UARTSELF ?= ./tb/uart_test

.PHONY: help nscscc-func nscscc-perf nscscc-tlb uart-test

help:
	@echo "make -C tb nscscc-func   NSCSCC functional test (golden trace)"
	@echo "make -C tb nscscc-perf   NSCSCC performance benchmark"
	@echo "make -C tb nscscc-tlb    NSCSCC TLB test"
	@echo "make -C tb uart-test     uart16550 self-test"

nscscc-func:
	cd $(ROOT) && $(NSCSCC) func

nscscc-perf:
	@read -p "Switch value: " sw; cd $(ROOT) && printf '%s\n' "$$sw" | $(NSCSCC) perf

nscscc-tlb:
	cd $(ROOT) && $(NSCSCC) tlb

uart-test:
	cd $(ROOT) && $(UARTSELF)
