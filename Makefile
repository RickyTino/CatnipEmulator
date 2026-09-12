# Project: CatnipVM

CPP      = g++
OBJ      = axi.o main.o mips32_core.o mips32_cp0.o bit_utils.o mips32_tlb.o mips32_tracer.o soc_lite.o
LINKOBJ  = axi.o main.o mips32_core.o mips32_cp0.o bit_utils.o mips32_tlb.o mips32_tracer.o soc_lite.o
LIBS     = -static-libgcc -g3
BIN      = soc_lite
CXXFLAGS = -g3 -std=c++11
RM       = rm -f

# Set OS_IS_WINDOWS=1 to build the Windows console-color path:
#   make OS_IS_WINDOWS=1
ifdef OS_IS_WINDOWS
CXXFLAGS += -DOS_IS_WINDOWS
endif

.PHONY: all clean

all: $(BIN)

clean:
	${RM} $(OBJ) $(BIN)

$(BIN): $(OBJ)
	$(CPP) $(LINKOBJ) -o $(BIN) $(LIBS)

axi.o: axi.cpp
	$(CPP) -c axi.cpp -o axi.o $(CXXFLAGS)

main.o: main.cpp
	$(CPP) -c main.cpp -o main.o $(CXXFLAGS)

mips32_core.o: mips32_core.cpp
	$(CPP) -c mips32_core.cpp -o mips32_core.o $(CXXFLAGS)

mips32_cp0.o: mips32_cp0.cpp
	$(CPP) -c mips32_cp0.cpp -o mips32_cp0.o $(CXXFLAGS)

bit_utils.o: bit_utils.cpp
	$(CPP) -c bit_utils.cpp -o bit_utils.o $(CXXFLAGS)

mips32_tlb.o: mips32_tlb.cpp
	$(CPP) -c mips32_tlb.cpp -o mips32_tlb.o $(CXXFLAGS)

mips32_tracer.o: mips32_tracer.cpp
	$(CPP) -c mips32_tracer.cpp -o mips32_tracer.o $(CXXFLAGS)

soc_lite.o: soc_lite.cpp
	$(CPP) -c soc_lite.cpp -o soc_lite.o $(CXXFLAGS)
