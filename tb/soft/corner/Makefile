# Assemble the corner-test guest programs into the raw images tb/corner_tests
# stages at run time.
#
#   make -C tb soft-corner        (or: make -C tb/soft/corner)
#
# The images are checked in, so this only needs to run after editing a .s - and
# it needs binutils for mipsel.  CROSS_COMPILE follows the convention of
# tb/soft/func/Makefile, with the prefix this workspace actually has.
#
# Two traps are baked in below, both met the hard way:
#
#   * --only-section=.text: every section of a relocatable object sits at
#     VMA 0, so a plain "objcopy -O binary" writes .reginfo (ABI metadata) in
#     front of the code and the guest ends up executing that.
#   * div: binutils 2.34 macro-expands the two-operand "div $rs,$rt" into a
#     run-time zero check plus "break 7", which intercepts the divisor before
#     the core ever sees it.  Cases that test division write the encoding as a
#     raw .word instead.
#
# The first word of every image is printed, so a bad image is obvious.

# (No plain "AS ?=" here: make predefines AS to the host assembler, so that
# would quietly build for the wrong target.)
CROSS_COMPILE ?= mipsel-linux-gnu-

SRCS := $(wildcard *.s)
BINS := $(SRCS:.s=.bin)

.PHONY: all clean
all: $(BINS)

%.bin: %.s
	$(CROSS_COMPILE)as -march=mips32 -o $*.o $<
	$(CROSS_COMPILE)objcopy -O binary --only-section=.text $*.o $@
	@printf '%-24s %4s bytes, first word: ' $@ "$$(stat -c%s $@)"
	@od -An -tx4 -N4 $@

clean:
	$(RM) $(BINS) $(SRCS:.s=.o)
