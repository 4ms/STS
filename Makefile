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
#CC = colorgcc
#CC = gccfilter -a -c $(ARCH)-gcc
CC = $(ARCH)-gcc
##Use -gcc instead of -ld
LD = $(ARCH)-gcc -Wl,-Map,build/main.map
#LD = $(ARCH)-ld -v -Map main.map
AS = $(ARCH)-as
OBJCPY = $(ARCH)-objcopy
OBJDMP = $(ARCH)-objdump
GDB = $(ARCH)-gdb
SZ = $(ARCH)-size

SZOPTS = -d

C0FLAGS  = -O0 -g -Wall
C0FLAGS += -mlittle-endian -mthumb 
C0FLAGS +=  -I. -DARM_MATH_CM4 -D'__FPU_PRESENT=1'  $(INCLUDES)  -DUSE_STDPERIPH_DRIVER
C0FLAGS += -mcpu=cortex-m4 -mfloat-abi=hard
C0FLAGS +=  -mfpu=fpv4-sp-d16 -fsingle-precision-constant -Wdouble-promotion 


#CFLAGS = -g2 -O1 \
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
CFLAGS = -O3 -g2 -fno-tree-loop-distribute-patterns -fno-schedule-insns  -fno-schedule-insns2 


CFLAGS += -mlittle-endian -mthumb 
CFLAGS += -I. -DARM_MATH_CM4 -D'__FPU_PRESENT=1'  $(INCLUDES)  -DUSE_STDPERIPH_DRIVER
CFLAGS += -mcpu=cortex-m4 -mfloat-abi=hard
CFLAGS += -mfpu=fpv4-sp-d16 -fsingle-precision-constant -Wdouble-promotion 
CFLAGS += -fstack-usage -fstack-check
#CFLAGS += --specs=rdimon.specs -lgcc -lc -lm -lrdimon

AFLAGS  = -mlittle-endian -mthumb -mcpu=cortex-m4 

LDSCRIPT = $(DEVICE)/$(LOADFILE)

#Use fpu/nosys.specs for standard C functions such as malloc(), memcpy()
LFLAGS  =  -mfloat-abi=hard --specs="fpu/nosys.specs" -nostartfiles -T $(LDSCRIPT) 


#vpath %.c src

# compile unoptimized: 
#build/src/main.o: CFLAGS = $(C0FLAGS)
#build/src/sts_filesystem.o: CFLAGS = $(C0FLAGS) 
#build/src/file_util.o: CFLAGS = $(C0FLAGS)
#build/src/wavefmt.o: CFLAGS = $(C0FLAGS)
#build/src/sample_file.o: CFLAGS = $(C0FLAGS)
#build/src/resample.o: CFLAGS = $(C0FLAGS)
#build/src/sampler.o: CFLAGS = $(C0FLAGS)
#build/src/params.o: CFLAGS = $(C0FLAGS)
#build/src/edit_mode.o: CFLAGS = $(C0FLAGS)
#build/src/flash_user.o: CFLAGS = $(C0FLAGS)
#build/src/calibration.o: CFLAGS = $(C0FLAGS)
#build/src/buttons.o: CFLAGS = $(C0FLAGS)
#build/src/dig_pins.o: CFLAGS = $(C0FLAGS)
#build/src/rgb_leds.o: CFLAGS = $(C0FLAGS)
#build/src/circular_buffer.o: CFLAGS = $(C0FLAGS)
#build/src/audio_util.o: CFLAGS = $(C0FLAGS)
#build/src/wav_recording.o: CFLAGS = $(C0FLAGS)
#build/src/sts_fs_index.o: CFLAGS = $(C0FLAGS)
#build/src/buttons.o: CFLAGS = $(C0FLAGS)
#build/src/file_util.o: CFLAGS = $(C0FLAGS)
#build/src/fatfs/ff.o: CFLAGS = $(C0FLAGS)
#build/src/fatfs/drivers/fatfs_sd_sdio.o: CFLAGS = $(C0FLAGS)
#build/src/fatfs/diskio.o: CFLAGS = $(C0FLAGS)
#build/src/fatfs/drivers/stm32f4_discovery_sdio_sd.o: CFLAGS = $(C0FLAGS)
#build/src/system_settings.o: CFLAGS = $(C0FLAGS)

all: Makefile $(BIN) $(HEX)

$(BIN): $(ELF)
	$(OBJCPY) -O binary $< $@
	$(OBJDMP) -x --syms $< > $(addsuffix .dmp, $(basename $<))
	ls -l $@ $<


$(HEX): $(ELF)
	$(OBJCPY) --output-target=ihex $< $@
	$(SZ) $(SZOPTS) $(ELF)

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
