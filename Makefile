#Based on https://github.com/nitsky/stm32-example 
#Modified by Dan Green http://github.com/4ms

BINARYNAME = main

STARTUP = startup_stm32f427_437xx.s
SYSTEM = system_stm32f4xx.c
LOADFILE = stm32f427.ld

DEVICE = stm32/device
CORE = stm32/core
PERIPH = stm32/periph

BUILDDIR = build

SOURCES += $(wildcard $(PERIPH)/src/*.c)
SOURCES += $(DEVICE)/src/$(STARTUP)
SOURCES += $(DEVICE)/src/$(SYSTEM)
SOURCES += $(wildcard src/*.c)
SOURCES += $(wildcard src/fatfs/*.c)
SOURCES += $(wildcard src/fatfs/drivers/*.c)
SOURCES += $(wildcard src/fatfs/option/*.c)

OBJECTS = $(addprefix $(BUILDDIR)/, $(addsuffix .o, $(basename $(SOURCES))))

INCLUDES += -I$(DEVICE)/include \
			-I$(CORE)/include \
			-I$(PERIPH)/include \
			-I inc \
			-I inc/res \
			-I inc/fatfs \
			-I inc/fatfs/drivers


ELF = $(BUILDDIR)/$(BINARYNAME).elf
HEX = $(BUILDDIR)/$(BINARYNAME).hex
BIN = $(BUILDDIR)/$(BINARYNAME).bin

ARCH = arm-none-eabi
CC = $(ARCH)-gcc
##Use -gcc instead of -ld
LD = $(ARCH)-gcc
#LD = $(ARCH)-ld -v -Map main.map
AS = $(ARCH)-as
OBJCPY = $(ARCH)-objcopy
OBJDMP = $(ARCH)-objdump
GDB = $(ARCH)-gdb

 	
C0FLAGS  = -O0 -g -Wall
C0FLAGS += -mlittle-endian -mthumb 
C0FLAGS +=  -I. -DARM_MATH_CM4 -D'__FPU_PRESENT=1'  $(INCLUDES)  -DUSE_STDPERIPH_DRIVER
C0FLAGS += -mcpu=cortex-m4 -mfloat-abi=hard
C0FLAGS +=  -mfpu=fpv4-sp-d16 -fsingle-precision-constant -Wdouble-promotion 


CFLAGS = -g2 -O1 \
          -fthread-jumps \
          -falign-functions  -falign-jumps \
          -falign-loops  -falign-labels \
          -fcaller-saves \
          -fcrossjumping \
          -fcse-follow-jumps  -fcse-skip-blocks \
          -fdelete-null-pointer-checks \
          -fexpensive-optimizations \
          -fgcse  -fgcse-lm  \
          -findirect-inlining \
          -foptimize-sibling-calls \
          -fpeephole2 \
          -fregmove \
          -freorder-blocks  -freorder-functions \
          -frerun-cse-after-loop  \
          -fsched-interblock  -fsched-spec \
          -fstrict-aliasing -fstrict-overflow \
          -ftree-switch-conversion \
          -ftree-pre \
          -ftree-vrp \
          -finline-functions -funswitch-loops -fpredictive-commoning -fgcse-after-reload -ftree-vectorize
          
# Causes Freeze on run: -fschedule-insns  -fschedule-insns2 
# CFLAGS = -O3 -fno-tree-loop-distribute-patterns 

CFLAGS += -mlittle-endian -mthumb 
CFLAGS +=  -I. -DARM_MATH_CM4 -D'__FPU_PRESENT=1'  $(INCLUDES)  -DUSE_STDPERIPH_DRIVER
CFLAGS += -mcpu=cortex-m4 -mfloat-abi=hard
CFLAGS +=  -mfpu=fpv4-sp-d16 -fsingle-precision-constant -Wdouble-promotion 
#CFLAGS += --specs=rdimon.specs -lgcc -lc -lm -lrdimon

AFLAGS  = -mlittle-endian -mthumb -mcpu=cortex-m4 

LDSCRIPT = $(DEVICE)/$(LOADFILE)

#Use FPU libraries (standard C)
LDLIBS = -L"/usr/local/Caskroom/gcc-arm-embedded/5_4-2016q3,20160926/gcc-arm-none-eabi-5_4-2016q3/arm-none-eabi/lib/fpu" -L"/usr/local/Caskroom/gcc-arm-embedded/5_4-2016q3,20160926/gcc-arm-none-eabi-5_4-2016q3/lib/gcc/arm-none-eabi/5.4.1/fpu"

#Use nosys.specs for standard C functions such as malloc(), memcpy()
LFLAGS  =  $(LDLIBS) --specs=nosys.specs -nostartfiles -T $(LDSCRIPT) 


#vpath %.c src

build/src/params.o: CFLAGS = $(C0FLAGS)
build/src/edit_mode.o: CFLAGS = $(C0FLAGS)
#build/src/resample.o: CFLAGS = $(C0FLAGS)
#build/src/sampler.o: CFLAGS = $(C0FLAGS)
#build/src/sts_filesystem.o: CFLAGS = $(C0FLAGS)
#build/src/wav_recoding.o: CFLAGS = $(C0FLAGS)
#build/src/buttons.o: CFLAGS = $(C0FLAGS)
#build/src/file_util.o: CFLAGS = $(C0FLAGS)
#build/src/fatfs/ff.o: CFLAGS = $(C0FLAGS)
#build/src/fatfs/diskio.o: CFLAGS = $(C0FLAGS)
#build/src/fatfs/drivers/fatfs_sd_sdio.o: CFLAGS = $(C0FLAGS)
#build/src/fatfs/diskio.o: CFLAGS = $(C0FLAGS)

all: Makefile $(BIN) $(HEX)

$(BIN): $(ELF)
	$(OBJCPY) -O binary $< $@
	$(OBJDMP) -x --syms $< > $(addsuffix .dmp, $(basename $<))
	ls -l $@ $<

$(HEX): $(ELF)
	$(OBJCPY) --output-target=ihex $< $@

$(ELF): $(OBJECTS) 
	$(LD) $(LFLAGS) -o $@ $(OBJECTS)


$(BUILDDIR)/%.o: %.c $(wildcard inc/*.h) $(wildcard inc/res/*.h)
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@


$(BUILDDIR)/%.o: %.s
	mkdir -p $(dir $@)
	$(AS) $(AFLAGS) $< -o $@ > $(addprefix $(BUILDDIR)/, $(addsuffix .lst, $(basename $<)))


flash: $(BIN)
	st-flash write $(BIN) 0x8000000

clean:
	rm -rf build
	
wav: fsk-wav

qpsk-wav: $(BIN)
	python stm_audio_bootloader/qpsk/encoder.py \
		-t stm32f4 -s 48000 -b 12000 -c 6000 -p 256 \
		$(BIN)

fsk-wav: $(BIN)
	python stm_audio_bootloader/fsk/encoder.py \
		-s 48000 -b 16 -n 8 -z 4 -p 256 -g 16384 -k 1800 \
		$(BIN)
