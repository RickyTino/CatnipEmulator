# CatnipEmulator - build rules.
#
#   make                 build every program (cemu, nscscc_tests, uart_test)
#   make cemu            the CatnipSoC emulator (loads and runs images)
#   make nscscc_tests    NSCSCC contest test harness (func/perf/tlb)
#   make uart_test       standalone uart16550 self-test
#   make OPT=-O0         build without optimization (for debugging)
#   make OS_IS_WINDOWS=1 build the Windows console-colour path
#
# Binaries land in the project root.  To run them, see run.mk (make -f run.mk).

CPP       = g++
SRC_DIR   = src
TB_DIR    = tb
OBJ_DIR   = build

# Source subdirectories.  Headers live next to their sources, and every
# subdirectory is on the include path so plain #include "foo.h" works.
SRC_SUBDIRS = common bus cpu dev soc

OPT      ?= -O2
VPATH     = $(SRC_DIR) $(addprefix $(SRC_DIR)/,$(SRC_SUBDIRS)) $(TB_DIR)
CXXFLAGS  = -g3 $(OPT) -std=c++11 -MMD -MP -I$(SRC_DIR) \
            $(addprefix -I$(SRC_DIR)/,$(SRC_SUBDIRS)) -I$(TB_DIR)
LIBS      = -static-libgcc -g3
RM        = rm -f

# --- cemu (the CatnipSoC emulator) ------------------------------------------
CEMU_SRC  = main catnipsoc image_loader axi mips32_core mips32_cp0 \
            mips32_tlb mips32_tracer uart16550
CEMU_OBJ  = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(CEMU_SRC)))
CEMU_BIN  = cemu

# --- nscscc_tests (functional / performance / TLB tests) --------------------
NSC_SRC   = nscscc_tests image_loader axi mips32_core mips32_cp0 \
            mips32_tlb mips32_tracer
NSC_OBJ   = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(NSC_SRC)))
NSC_BIN   = nscscc_tests

# --- uart_test (uart16550 self-test) ----------------------------------------
UART_SRC  = uart_test uart16550 axi
UART_OBJ  = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(UART_SRC)))
UART_BIN  = uart_test

DEPS      = $(CEMU_OBJ:.o=.d) $(NSC_OBJ:.o=.d) $(UART_OBJ:.o=.d)

# Set OS_IS_WINDOWS=1 to build the Windows console-colour path:
#   make OS_IS_WINDOWS=1
ifdef OS_IS_WINDOWS
CXXFLAGS += -DOS_IS_WINDOWS
endif

.PHONY: all clean

# $(CEMU_BIN), $(NSC_BIN) and $(UART_BIN) are the binaries themselves in the
# project root, so "make cemu" / "make nscscc_tests" / "make uart_test" work
# directly.
all: $(CEMU_BIN) $(NSC_BIN) $(UART_BIN)

$(CEMU_BIN): $(CEMU_OBJ)
	$(CPP) $(CEMU_OBJ) -o $@ $(LIBS)

$(NSC_BIN): $(NSC_OBJ)
	$(CPP) $(NSC_OBJ) -o $@ $(LIBS)

$(UART_BIN): $(UART_OBJ)
	$(CPP) $(UART_OBJ) -o $@ $(LIBS)

# Sources are found through $(VPATH) (src/*/ and tb/).
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(OBJ_DIR)
	$(CPP) -c $< -o $@ $(CXXFLAGS)

clean:
	$(RM) -r $(OBJ_DIR)
	$(RM) $(CEMU_BIN) $(NSC_BIN) $(UART_BIN)

# Auto-generated header dependencies (-MMD -MP).  Harmless if not present yet.
-include $(DEPS)
