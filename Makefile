CC = avr-gcc
OBJCOPY = avr-objcopy
SIZE = avr-size
NM = avr-nm
AVRDUDE = avrdude
REMOVE = rm -f

MCU = attiny85
F_CPU = 8000000

LFUSE = 0xe2
HFUSE = 0xdf

TARGET = SpeedCorrector
SRC = SpeedCorrector.c
OBJ = $(SRC:.S=.o)
LST = $(SRC:.S=.lst)

FORMAT = ihex

OPTLEVEL = s

CDEFS = 

CFLAGS = -DF_CPU=$(F_CPU)UL
CFLAGS += $(CDEFS)
CFLAGS += -O$(OPTLEVEL)
CFLAGS += -mmcu=$(MCU) -g
#CFLAGS += -std=gnu99
#CFLAGS += -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
#CFLAGS += -ffunction-sections -fdata-sections
#CFLAGS += -Wall -Wstrict-prototypes
#CFLAGS += -Wa,-adhlns=$(<:.c=.lst)

LDFLAGS = -Wl,--gc-sections
LDFLAGS += -Wl,--print-gc-sections

AVRDUDE_MCU = attiny85 
AVRDUDE_PROGRAMMER = usbtiny
AVRDUDE_SPEED = -B 125kHz

AVRDUDE_FLAGS = -p $(AVRDUDE_MCU)
AVRDUDE_FLAGS += -c $(AVRDUDE_PROGRAMMER)
AVRDUDE_FLAGS += $(AVRDUDE_SPEED)

MSG_LINKING = Linking:
MSG_COMPILING = Compiling:
MSG_FLASH = Preparing HEX file:

all: gccversion $(TARGET).elf $(TARGET).hex size

.SECONDARY: $(TARGET).elf
.PRECIOUS: $(OBJ)

%.hex: %.elf
	@echo
	@echo $(MSG_FLASH) $@
	$(OBJCOPY) -O $(FORMAT) -j .text -j .data $< $(@F)

%.elf : %.c
	@echo $(MSG_COMPILING) $<
	$(CC) $(CFLAGS) $< -o $(@F)

%.elf : %.S
	@echo "Running Assembler:" $<
	$(CC) $(CFLAGS) -nostdlib $< -o $(@F)

gccversion:
	@$(CC) --version

size: $(TARGET).elf
	@echo
	$(SIZE) -C --mcu=$(AVRDUDE_MCU) $(TARGET).elf

analyze: $(TARGET).elf
	$(NM) -S --size-sort -t decimal $(TARGET).elf

isp: $(TARGET).hex
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$(TARGET).hex

fuses:
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m

release: fuses isp

clean:
	$(REMOVE) $(TARGET).hex $(TARGET).elf $(OBJ) $(LST) *~
