# STS
## Stereo Triggered Sampler

Firmware for the Stereo Triggered Sampler, from 4ms Company.

The hardware contains:

*	180MHz 32-bit ARM chip with FPU (STM32F427)
*	Two Stereo codec chips, very low noise and low jitter (CS4721)
	*	Total of four audio inputs and four audio outputs
*	Eight potentiometers
*	Six Analogue inputs:
	*	Four unipolar analogue inputs (0 to 5V)
	*	Two bipolar analogue inputs (-5V to 5V)
*	Five LED buttons (momentary)
*	Two three-position switches
*	Five trigger/gate digital inputs
*	Three digital outputs
*	Two LEDs, capable of being dimmed with PWM
*	Separate analogue power supplies for each codec, and also the ADC 
*	Efficient 3.3V DC-DC converter accepts a wide range of input voltages
 

## Setting up your environment
You need to install the GCC ARM toolchain.
This project is known to compile with arm-none-eabi-gcc version 7.3.1, version 8.2.1, version 9.2.1, version 10.2.1, and 10.3.1. It's likely to compile with other versions as well.

It's recommended (but not necessary) to install ST-UTIL/stlink. Without it, you will have to update using the audio bootloader, which is very slow (5 minutes per update).
With ST-UTIL or stlink and a programmer, you can update in 5-20 seconds.
The Texane stlink package contains a gdb debugger, which works with ST-LINK programmers such as the [STM32 Discovery boards](http://www.mouser.com/ProductDetail/STMicroelectronics/STM32F4DISCOVERY/?qs=J2qbEwLrpCGdWLY96ibNeQ%3D%3D&gclid=CKb6u6Cz48cCFZGBfgodfHwH-g&kpid=608656256) to connect to the Spectral's 4-pin SWD header. The STM32F4 Discovery Board is low-cost (under US$15) and works great as a programmer and debugger.

You also may wish to install an IDE such as Eclipse. There are many resources online for setting up GCC ARM with Eclipse (as well as commerical software). This is totally optional. Instead of an IDE you can use your favorite text editor and a few commands on the command line (Terminal) which are given below.

Continue below for Max OSX, Linux, or Windows instructions.


### Mac OSX

For Mac OSX, follow these instructions to install brew and then the arm toolchain and st-link (taken from https://github.com/nitsky/homebrew-stm32):

	ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
	brew tap nitsky/stm32
	brew install arm-none-eabi-gcc
	brew install stlink
	
Unfortunately `brew install arm-none-eabi-gcc` will download an old version of arm-none-eabi-gcc. This will work fine, so you can stop here if you want to do the easiest thing possible. But if you want to develop new firmware using C++17 or C++20 features, you'll need a newer toolchain. You can get a later version of arm-none-eabi-gcc [from ARM's website here](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads). After you unzip the download, add the `bin/` directory to your `PATH` like this:

	export PATH=$PATH:(path to the unzipped gcc-arm-none-eabi folder)/bin

Alternatively, if you don't want to mess with your PATH, you can export an environment variable that the Makefile will look for, like this (note the trailing slash):

	export TOOLCHAIN_DIR=(path to the unzipped gcc-arm-none-eabi folder)/bin/

That's it! Continue below to **Clone the Projects**

### Linux

For linux, check your package manager to see if there is a package for arm-none-eabi-gcc or gcc-arm-embedded or gcc-arm-none-eabi. Or just download it here:

[Download GCC ARM toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads)

Next, install st-link from texane:

	sudo apt-get install git libusb-1.0.0-dev pkg-config autotools-dev
	cd (your work directory)
	git clone https://github.com/texane/stlink.git
	cd stlink
	./autogen.sh
	./configure
	make
	export PATH=$PATH:(your work directory)/stlink

The last command makes sure that the binary st-flash is in your PATH so that the Makefile can run it to program your module. Alternatively you can omit the last command and run this instead:

	export TOOLCHAIN_DIR=(path to the unzipped gcc-arm-none-eabi folder)/bin/

The above environment variable will be used by the STS's Makefile to find the compiler.
 
That's it! Continue below to **Clone the Projects**

### Windows
[Download GCC ARM toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads)

[Download ST-UTIL](http://www.st.com/web/en/catalog/tools/PF258168)

Install both. Please contact me if you run into problems that you can't google your way out of! 


## Clone the Projects

Make sure git is installed on your system. (OSX: type `brew install git` into the Terminal)

Create a work directory, and enter it.

Clone this project (STS) and the required submodule (which is libhwtest)

	git clone https://github.com/4ms/STS.git
	cd STS
	git submodule update --init
	
If you don't need to change the bootloader firmware or generate a .wav file for updating firmware, then you can skip to **Compiling**.

Otherwise, if you want to modify the audio bootloader firmware, or create .wav files for sharing your custom firmware with others, then clone these projects as well:

	cd ..         ## if you were in the STS dir, go up one level
	git clone https://github.com/4ms/sts-bootloader.git  
	git clone https://github.com/4ms/stmlib.git
	git clone https://github.com/4ms/stm-audio-bootloader.git
	
Create a symlink for stm-audio-bootloader so that it works with python (required to generate the .wav file for audio bootloading)

     ln -s stm-audio-bootloader stm_audio_bootloader

Verify that your directories are as follows:

	(work directory)
	|  
	|--STS/  
	|--sts-bootloader/  
	|--stm-audio-bootloader/  
	|--stm_audio_bootloader/   <----symlink to stm-audio-bootloader
	|--stmlib/



## Compiling
Make your changes to the code in the STS directory. When ready to compile, build the project like this (make sure you're in the `STS/` directory)

	make

This creates an main.elf, main.bin, and main.hex file in the build/ directory.


## Programmer (Hardware) ##

Once you compile your firmware, you need to flash it to your STS. You will need a hardware device that connects to your computer's USB port. The other side of the programmer has a header that connects to the STS's 4-pin SWD header.

You have several options:

### Option 1) ST-LINKv2 programmer ###

The [ST-LINK v2 from ST corporation](http://www.mouser.com/search/ProductDetail.aspx?R=0virtualkey0virtualkeyST-LINK-V2) is the offical programmer from ST. The ST-LINK v2 has a 20-pin JTAG header, and you need to connect four of these to your STS. More details on this are below.

The ST-LINKv2 programmer can be used with ST-LINK software for Mac, Linux, or Windows; or ST-UTIL software for Windows; or STM32CubeProgrammer software for any platform. 

### Option 2) Discovery board programmer ###

The [STM32 Discovery boards](http://www.mouser.com/search/ProductDetail.aspx?R=0virtualkey0virtualkeySTM32F407G-DISC1) are low-cost (around US$25) and work great as a programmer and debugger. The 'Disco' board is essentially an ST-LINKv2, plus some extra stuff. While the ST-LINK v2 is encased in a plastic enclosure, the Discovery board is open and could potentially be damaged if you're not careful. Read the Discovery board's manual to learn about setting the jumpers to use it as an SWD programmer (rather than an evaluation board). ST makes many variations of the Discovery board, and to my knowledge they all contain an ST-LINK programmer.

The Discovery board programmer is essentially an ST-LINKv2, so it can be used with all the same software that the ST-LINKv2 can (see above section).

### Option 3) SEGGER's J-Link or J-Trace ###

Another option is [SEGGER's J-link programmer](https://www.segger.com/jlink-debug-probes.html). There is an educational version which is very affordable and a good choice for a programmer if you meet the requirements for educational use. There are various professional commercial versions with a number of useful features. The J-link uses SEGGER's propriety software, Ozone, which not only will flash a hex or bin file, but can be used as a powerful GUI debugger if you use it to open the .elf file. There also are command-line commands for flashing.
J-Link software runs on Mac, Linux, and Windows and is free of cost.

### Option 4) Audio Bootloader ###

The STS has an audio bootloader built-in, so you can just compile your project using `make wav` and then play the wav file into the audio input of the STS while in bootloader mode (see STS User Manual for detailed procedure). No additional hardware or software is necessary.

This works well and is the safest option. However it's very slow (up to 5 minutes per update). If you are going to be making a series of changes, this will be a very slow process! However if you want to share your custom firmware with others, it's a great way to package it since users don't need any special devices to update.

When ready to build an audio file for the bootloader, make it like this:

	make wav

This requires python to run. It creates the file `main.wav` in the `build/` directory. Play the file from a computer or device into the STS by following the instructions in the User Manual on the [4ms STS page](http://4mscompany.com/STS). 


## Programmer (Software)

Depending on the hardware you chose, one or more of the following software options will work:

### ST-LINK ###

This software works with the ST-LINK v2 and the Discovery board to flash your STS.

__MacOS:__

```
brew install stlink
```

__Linux:__

Use your package manager to install `stlink-tools`. See [stlink README](https://github.com/stlink-org/stlink) for direct links.

__Windows:__

Download the [latest binary (v1.3.0)](https://github.com/stlink-org/stlink/releases/download/v1.3.0/stlink-1.3.0-win64.zip). If that link doesn't work anymore, or you want to try a newer version, go to [stlink-org/stlink github page](https://github.com/stlink-org/stlink).

__Programming using st-link__

You can flash the compiled .bin file by typing either

	st-flash write build/main.bin 0x08008000

or the shortcut:

	make stflash
	
This writes the main.bin file starting at the 0x08008000 sector, which is the first sector of the application code.

__Debugging__

The gcc-arm toolchain includes a gdb debugger, but you'll need a server to use it. The st-link software includes st-util which will provide this. OpenOCD and J-link are two other options. Configuring debugging is beyond the scope of this README, but there are plenty of tutorials online. See the [stlink README](https://github.com/stlink-org/stlink) for a quick start.


### J-Link/Ozone ###

If you're using a J-Link or J-Trace hardware programmer to flash your STS, download and install [Ozone](https://www.segger.com/downloads/jlink). Ozone is a GUI debugger which can program, too. For command-line options, consider [J-Link Software and Documentation Pack](https://www.segger.com/j-link-software.html), which contains the GDB server and other tools.

To program your STS, open Ozone and then use the Project Wizard to create a new project. Select the STM32F427ZG for the chip. Use the default settings elsewhere. On the page that lets you select a file, select the main.elf file that you compiled. You can then save this project. To program, just click the green power/arrow symbol at the upper left.

You also can use the CLI tools to flash the hex/elf file, see J-Link documentation for details. The Makefile contains a shortcut for this:

	make flash

J-Link and Ozone is available for Mac, Windows, and Linux.

### STM32CubeProgrammer ###

The STM32CubeProgrammer is ST's official programaming software available for all platforms. [You can find it here](https://www.st.com/en/development-tools/stm32cubeprog.html)

Documentation is included with the download.


### ST-UTIL (Windows only) ###

ST-UTIL is a program from ST that runs on Windows only. You can find it on [ST.com](https://my.st.com/content/my_st_com/en/products/development-tools/software-development-tools/stm32-software-development-tools/stm32-programmers/stsw-link004.html)

It works with the ST-LINKv2 and Discovery boards.


## Connecting the programmer to the STS ##

**Discovery Board**: The Discovery board has a 6-pin SWD header, and you can connect that pin-for-pin to the first 4 pins on the STS's SWD header. You can use a 0.1" spaced 4-pin Jumper Wire such as [this one from Sparkfun](https://www.sparkfun.com/products/10364). Pay attention to Pin 1 (marked by "1" on the STS board), or you risk damaging the ARM chip.

**ST-LINK v2 or SEGGER J-Link**: Both the ST-LINK and the SEGGER J-link have a 20-pin JTAG connector. You need to connect 4 of these pins to the STS's 4-pin SWD connector, using 4 wires: [here's a pack of 10](https://www.sparkfun.com/products/8430). Pay attention to Pin 1 (marked by "1" on the STS board), or you risk damaging the ARM chip.
 
Look at these images:
 
  * JTAG: [20-pin pinout](http://www.jtagtest.com/pinouts/arm20)
  * SWD: [6-pin pinout](https://wiki.paparazziuav.org/wiki/File:Swd_header_discovery_board.png)
  * JTAG-to-SWD: [JTAG-to-SWD-pinout](https://4mscompany.com/JTAG-to-SWD-pinout.png). 

Then use your jumper wires to connect:
 
  * SWD pin 1 (VDD) -> JTAG pin 1 (VREF)
  * SWD pin 2 (SWCLK) -> JTAG pin 9 (TCK)
  * SWD pin 3 (GND) -> JTAG pin 4 (GND)
  * SWD pin 4 (SWDIO) -> JTAG pin 7 (TMS)

Here's more info in case you want it explained in another way:

  * [This image](https://www.alexwhittemore.com/wp-content/uploads/2014/08/schematic.jpg) draws this out. (Taken from [this post](https://www.alexwhittemore.com/st-linkv2-swd-jtag-adapter/))
  * [ST's manual](http://www.st.com/content/ccc/resource/technical/document/user_manual/70/fe/4a/3f/e7/e1/4f/7d/DM00039084.pdf/files/DM00039084.pdf/jcr:content/translations/en.DM00039084.pdf)
  * [How to make an adaptor](http://gnuarmeclipse.github.io/developer/j-link-stm32-boards/#the-st-6-pin-swd-connector)
  * Make your own adaptor from these gerber files: [JTAG-SWD-adaptor](https://github.com/4ms/pcb-JTAG-SWD-adaptor).

### Troubleshooting: ###

If you have trouble getting python to create a wav file, such as this error:

	ImportError: No module named stm_audio_bootloader

Then try this command:
	
	export PYTHONPATH=$PYTHONPATH:'.'

## Bootloader
The bootloader is a [separate project](https://github.com/4ms/stm-audio-bootloader), slightly modifed from the stm-audio-bootloader from [pichenettes](https://github.com/pichenettes/eurorack). 

The bootloader is already installed on all factory-built STS units.

## License
The code (software) is licensed by the MIT license.

The hardware is licensed by the [CC BY-NC-SA license](https://creativecommons.org/licenses/by-nc-sa/4.0/) (Creative Commons, Attribution, NonCommercial, ShareAlike).

See LICENSE file.

I would like to see others build and modify the STS and STS-influenced works, in a non-commercial manner. My intent is not to limit the educational use nor to prevent people buying hardware components/PCBs collectively in a group. If you have any questions regarding the license or appropriate use, please do not hesitate to contact me! 

## Guidelines for derivative works

Do not include the text "4ms" or "4ms Company" or the graphic 4ms logo on any derivative works. This includes faceplates, enclosures, or front-panels. It's OK (but not required) to include the text "Stereo Triggered Sampler" or "STS" if you wish.
