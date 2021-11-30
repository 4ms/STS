BINARYNAME = main

STARTUP = startup_stm32f427_modern.s
SYSTEM = system_stm32f4xx.c
LOADFILE = stm32f427_modern.ld

COMBO = build/combo
BOOTLOADER_DIR = ../STS-bootloader
BOOTLOADER_HEX = ../STS-bootloader/bootloader.hex

DEVICE = stm32/device
CORE = stm32/core
PERIPH = stm32/periph

BUILDDIR = build

SOURCES += $(wildcard $(PERIPH)/src/*.c)
SOURCES += $(DEVICE)/src/$(STARTUP)
SOURCES += $(DEVICE)/src/$(SYSTEM)
SOURCES += $(wildcard src/*.c)
SOURCES += $(wildcard libhwtests/src/*.c)
SOURCES += $(wildcard libhwtests/src/*.cc)
SOURCES += $(wildcard libhwtests/src/*.cpp)
SOURCES += $(wildcard src/hardware_tests/*.c)
SOURCES += $(wildcard src/hardware_tests/*.cc)
SOURCES += $(wildcard src/hardware_tests/*.cpp)

SOURCES += $(wildcard src/fatfs/*.c)
SOURCES += $(wildcard src/fatfs/drivers/*.c)
SOURCES += $(wildcard src/fatfs/option/*.c)

OBJECTS = $(addprefix $(BUILDDIR)/, $(addsuffix .o, $(basename $(SOURCES))))
DEPS = $(OBJECTS:.o=.d)

# show:
# 	echo $(OBJECTS)

INCLUDES += -I$(DEVICE)/include \
			-I$(CORE)/include \
			-I$(PERIPH)/include \
			-Iinc \
			-Iinc/res \
			-Iinc/fatfs \
			-Iinc/fatfs/drivers \
			-Iinc/hardware_tests \
			-Ilibhwtests/inc \

ELF = $(BUILDDIR)/$(BINARYNAME).elf
HEX = $(BUILDDIR)/$(BINARYNAME).hex
BIN = $(BUILDDIR)/$(BINARYNAME).bin

TOOLCHAIN_DIR ?= 

ARCH = $(TOOLCHAIN_DIR)/arm-none-eabi
CC = $(ARCH)-gcc
CXX =$(ARCH)-g++
LD = $(ARCH)-g++
AS = $(ARCH)-as
OBJCPY = $(ARCH)-objcopy
OBJDMP = $(ARCH)-objdump
GDB = $(ARCH)-gdb

OPTFLAGS = -O3 -fno-tree-loop-distribute-patterns -fno-schedule-insns  -fno-schedule-insns2 

CFLAGS = -g2
CFLAGS += -mlittle-endian -mthumb 
CFLAGS += -mcpu=cortex-m4 
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16 
CFLAGS += -DARM_MATH_CM4 -D'__FPU_PRESENT=1' -DUSE_STDPERIPH_DRIVER
CFLAGS += -I. $(INCLUDES)
CFLAGS += -fno-exceptions -fsingle-precision-constant -Wdouble-promotion -fcommon
CFLAGS += -ffreestanding
CFLAGS += --specs=nosys.specs
CFLAGS += -DHSE_VALUE=16000000

CXXFLAGS = -std=c++17
CXXFLAGS += -Wno-register

AFLAGS  = -mlittle-endian -mthumb -mcpu=cortex-m4

LDSCRIPT = $(DEVICE)/$(LOADFILE)
LFLAGS  = $(CFLAGS) -Wl,-Map,main.map -T $(LDSCRIPT)

# Uncomment to compile unoptimized:


# Main:
# $(BUILDDIR)/src/main.o: OPTFLAGS = -O0

# STS Filesystem:
# $(BUILDDIR)/src/sts_filesystem.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/sts_fs_index.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/sample_file.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/wavefmt.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/file_util.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/bank.o: OPTFLAGS = -O0

# FAT Filesystem
# $(BUILDDIR)/src/fatfs/ff.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/fatfs/diskio.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/fatfs/drivers/stm32f4_discovery_sdio_sd.o: OPTFLAGS = -O0

# Playback/Record:
# $(BUILDDIR)/src/resample.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/sampler.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/wav_recording.o: OPTFLAGS = -O0

# Special Modes:
# $(BUILDDIR)/src/edit_mode.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/flash_user.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/calibration.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/system_mode.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/user_settings.o: OPTFLAGS = -O0

# I/O:
# $(BUILDDIR)/src/buttons.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/params.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/dig_pins.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/rgb_leds.o: OPTFLAGS = -O0

# Misc:
# $(BUILDDIR)/src/circular_buffer.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/audio_util.o: OPTFLAGS = -O0

# $(BUILDDIR)/src/hardware_tests/hardware_tests.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/hardware_tests/hardware_test_adc.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/hardware_tests/hardware_test_audio.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/hardware_tests/hardware_test_gate_outs.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/hardware_tests/hardware_test_gates.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/hardware_tests/hardware_test_switches.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/hardware_tests/hardware_test_sdcard.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/hardware_tests/hardware_test_util.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/hardware_tests/src/hardware_test_adc.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/hardware_test_switches_buttons.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/hardware_tests/hardware_test_gates.o: OPTFLAGS = -O0
# $(BUILDDIR)/src/hardware_tests/hardware_test_gate_outs.o: OPTFLAGS = -O0
# $(BUILDDIR)/libhwtests/src/GateInChecker.o: OPTFLAGS = -O0
# $(BUILDDIR)/libhwtests/src/GateOutput.o: OPTFLAGS = -O0

DEPFLAGS = -MMD -MP -MF $(BUILDDIR)/$(basename $<).d

all: Makefile $(BIN) $(HEX)

$(BIN): $(ELF)
	@$(OBJCPY) -O binary $< $@
	@$(OBJDMP) -x --syms $< > $(addsuffix .dmp, $(basename $<))
	ls -l $@ $<

$(HEX): $(ELF)
	@$(OBJCPY) --output-target=ihex $< $@

$(ELF): $(OBJECTS)
	@echo "Linking..."
	@$(LD) $(LFLAGS) -o $@ $(OBJECTS)

$(BUILDDIR)/%.o: %.c $(BUILDDIR)/%.d
	@mkdir -p $(dir $@)
	@echo "Compiling:" $<
	@$(CC) -c $(DEPFLAGS) $(OPTFLAGS) $(CFLAGS) $< -o $@

$(BUILDDIR)/%.o: %.cc $(BUILDDIR)/%.d
	@mkdir -p $(dir $@)
	@echo "Compiling:" $<
	@$(CXX) -c $(DEPFLAGS) $(OPTFLAGS) $(CFLAGS) $(CXXFLAGS) $< -o $@

$(BUILDDIR)/%.o: %.cpp $(BUILDDIR)/%.d
	@mkdir -p $(dir $@)
	@echo "Compiling:" $<
	@$(CXX) -c $(DEPFLAGS) $(OPTFLAGS) $(CFLAGS) $(CXXFLAGS) $< -o $@

$(BUILDDIR)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "Compiling:" $<
	@$(AS) $(AFLAGS) $< -o $@ > $(addprefix $(BUILDDIR)/, $(addsuffix .lst, $(basename $<)))

combo: $(COMBO).hex 
$(COMBO).hex:  $(BOOTLOADER_HEX) $(BIN) $(HEX) 
	cat  $(HEX) $(BOOTLOADER_HEX) | \
	awk -f $(BOOTLOADER_DIR)/util/merge_hex.awk > $(COMBO).hex
	$(OBJCPY) -I ihex -O binary $(COMBO).hex $(COMBO).bin


%.d: ;

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPS)
endif

flash: $(HEX)
	@printf "device STM32F427ZG\nspeed 4000kHz\nif SWD\nr\nloadfile ./build/main.hex\nr\nq" > flashScript.jlink
	JLinkExe -CommanderScript flashScript.jlink

eraseflash:
	@printf "device STM32F427ZG\nspeed 4000kHz\nif SWD\nr\nerase 0x08000000, 0x08100000\nq" > flashScript.jlink
	JLinkExe -CommanderScript flashScript.jlink

comboflash: $(COMBO).hex
	@printf "device STM32F427ZG\nspeed 4000kHz\nif SWD\nr\nloadfile $(COMBO).hex\nq" > flashScript.jlink
	JLinkExe -CommanderScript flashScript.jlink

stflash: $(BIN)
	st-flash write $(BIN) 0x8008000

clean:
	rm -rf build
	
wav: fsk-wav

fsk-wav: $(BIN)
	python2 stm_audio_bootloader/fsk/encoder.py \
		-s 44100 -b 16 -n 8 -z 4 -p 256 -g 16384 -k 1800 \
		$(BIN)

