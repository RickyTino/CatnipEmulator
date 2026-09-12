ifndef CROSS_COMPILE
CROSS_COMPILE = mipsel-linux-gnu-
endif

SOURCE = test.S
TARGET = tlb_test

AS      = $(CROSS_COMPILE)as
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

OBJECTS = $(TARGET).o

all: ../tlb_ram.txt

$(TARGET).o: $(SOURCE)
	$(AS) -mips32 $< -o $@

$(TARGET).elf: ram.ld $(TARGET).o
	$(LD) -T ram.ld $(TARGET).o -o $@
	$(OBJDUMP) -alD $@ > $(TARGET).s

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

../tlb_ram.txt: $(TARGET).bin
	python3 convert.py $(TARGET).bin $@

clean:
	rm -f *.o *.elf *.bin *.s ../tlb_ram.txt
