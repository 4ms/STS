#include "hardware_test_util.h"
#include "str_util.h"
extern "C" {
#include "LED_palette.h"
#include "dig_pins.h"
#include "ff.h"
#include "globals.h"
#include "pca9685_driver.h"
#include "sts_filesystem.h"
#include "stm32f4_discovery_sdio_sd.h"
#include "rgb_leds.h"
}

extern FATFS FatFs;
static void setup();
static void create_tmp_dir();
static bool create_tmp_file();
static bool read_tmp_file();

static const int datasize = 4096;
static const char tmp_name[7] = "hwtest";
static auto encode = [](int x){return static_cast<uint8_t>((x*7+3) & 0xFF);};

bool test_sdcard(void) {
	setup();
	create_tmp_dir();
	if (create_tmp_file()) {
		if (read_tmp_file())
			return true;
	}
	flash_mainbut_until_pressed();
	return false;
}

void setup() {
	LEDDriver_Init(2);
	LEDDRIVER_OUTPUTENABLE_ON;
	init_buttonLEDs();

	set_ButtonLED_byPalette(Bank1ButtonLED, GREEN);
	set_ButtonLED_byPalette(Bank2ButtonLED, GREEN);
	set_ButtonLED_byPalette(RecBankButtonLED, GREEN);
	set_ButtonLED_byPalette(RecButtonLED, GREEN);
	display_all_ButtonLEDs();

	reload_sdcard();

	set_ButtonLED_byPalette(Bank1ButtonLED, BLUE);
	set_ButtonLED_byPalette(Bank2ButtonLED, BLUE);
	set_ButtonLED_byPalette(RecBankButtonLED, BLUE);
	set_ButtonLED_byPalette(RecButtonLED, BLUE);
	display_all_ButtonLEDs();
}

void create_tmp_dir() {
	DIR dir;
	FRESULT res;

	//Open the temp directory
	res = f_opendir(&dir, TMP_DIR);
	if (res==FR_NO_PATH)
		res = f_mkdir(TMP_DIR);

	//Loop until successful
	while (res!=FR_OK)
	{
		res = reload_sdcard();
		if (res==FR_OK)
		{
			FRESULT open_res = f_opendir(&dir, TMP_DIR);
			if (open_res==FR_NO_PATH)
				res = f_mkdir(TMP_DIR);
		}

		set_ButtonLED_byPalette(RecBankButtonLED, RED);
		set_ButtonLED_byPalette(RecButtonLED, RED);
		display_all_ButtonLEDs();
		pause_until_button();

		set_ButtonLED_byPalette(RecBankButtonLED, OFF);
		set_ButtonLED_byPalette(RecButtonLED, OFF);
		display_all_ButtonLEDs();
	}
}

bool create_tmp_file() {
	set_ButtonLED_byPalette(Bank1ButtonLED, BLUE);
	set_ButtonLED_byPalette(Bank2ButtonLED, BLUE);
	set_ButtonLED_byPalette(RecBankButtonLED, OFF);
	set_ButtonLED_byPalette(RecButtonLED, OFF);
	display_all_ButtonLEDs();

	FIL testfil;
	char data[datasize];
	UINT bytes_written=0;

	for (int i = 0; i<datasize; i++)
		data[i] = encode(i);

	FRESULT res = f_open(&testfil, tmp_name, FA_WRITE | FA_OPEN_EXISTING| FA_CREATE_NEW | FA_READ);
	if (res==FR_EXIST) {
		res = f_unlink(tmp_name);
		if (res==FR_OK)
			res = f_open(&testfil, tmp_name, FA_WRITE | FA_OPEN_EXISTING| FA_CREATE_NEW | FA_READ);
	}
	if (res==FR_OK)
		res = f_write(&testfil, data, datasize, &bytes_written);
	else {
		//Fail: cannot create new file
		set_ButtonLED_byPalette(Bank1ButtonLED, RED);
		set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
		set_ButtonLED_byPalette(RecBankButtonLED, OFF);
		set_ButtonLED_byPalette(RecButtonLED, OFF);
		display_all_ButtonLEDs();
		return false;
	}
	if (res!=FR_OK) {
		//Fail: error when writing
		f_close(&testfil);
		set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
		set_ButtonLED_byPalette(Bank2ButtonLED, RED);
		set_ButtonLED_byPalette(RecBankButtonLED, OFF);
		set_ButtonLED_byPalette(RecButtonLED, OFF);
		display_all_ButtonLEDs();
		return false;
	}
	else if (bytes_written==0) {
		//Fail: file not written
		set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
		set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
		set_ButtonLED_byPalette(RecBankButtonLED, RED);
		set_ButtonLED_byPalette(RecButtonLED, OFF);
		display_all_ButtonLEDs();
		return false;
	}
	else if (bytes_written != datasize)
	{
		//Error: file only partially written, or not
		set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
		set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
		set_ButtonLED_byPalette(RecBankButtonLED, OFF);
		set_ButtonLED_byPalette(RecButtonLED, RED);
		display_all_ButtonLEDs();
		pause_until_button();
		return false;
	}

	f_close(&testfil);
	return true;
}

bool read_tmp_file() {
	char data[datasize];
	FIL testfil;
	UINT bytes_read;

	for (int i = 0; i<datasize; i++)
		data[i] = 0;

	//Read data back
	set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
	set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
	set_ButtonLED_byPalette(RecBankButtonLED, BLUE);
	set_ButtonLED_byPalette(RecButtonLED, BLUE);
	display_all_ButtonLEDs();
	
	FRESULT res = f_open(&testfil, tmp_name, FA_READ);
	if (res==FR_OK)
		res = f_read(&testfil, data, datasize, &bytes_read);
	else {
		//Fail: cannot open file
		set_ButtonLED_byPalette(Bank1ButtonLED, YELLOW);
		set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
		set_ButtonLED_byPalette(RecBankButtonLED, OFF);
		set_ButtonLED_byPalette(RecButtonLED, OFF);
		display_all_ButtonLEDs();
		pause_until_button();
		return false;
	}
	if (res!=FR_OK) {
		//Fail: error when reading
		f_close(&testfil);
		set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
		set_ButtonLED_byPalette(Bank2ButtonLED, YELLOW);
		set_ButtonLED_byPalette(RecBankButtonLED, OFF);
		set_ButtonLED_byPalette(RecButtonLED, OFF);
		display_all_ButtonLEDs();
		pause_until_button();
		return false;
	}
	if (bytes_read==0) {
		//Fail: nothing read from file
		set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
		set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
		set_ButtonLED_byPalette(RecBankButtonLED, YELLOW);
		set_ButtonLED_byPalette(RecButtonLED, OFF);
		display_all_ButtonLEDs();
		pause_until_button();
		return false;
	}
	if (bytes_read != datasize)
	{
		//Error: file only partially read
		set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
		set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
		set_ButtonLED_byPalette(RecBankButtonLED, OFF);
		set_ButtonLED_byPalette(RecButtonLED, YELLOW);
		display_all_ButtonLEDs();
		pause_until_button();
		return false;
	}

	f_close(&testfil);

	for (int i = 0; i<datasize; i++) {
		if (data[i] != encode(i)) {
			//Fail: bad data read back
			set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
			set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
			set_ButtonLED_byPalette(RecBankButtonLED, RED);
			set_ButtonLED_byPalette(RecButtonLED, YELLOW);
			display_all_ButtonLEDs();
			pause_until_button();
			return false;
		}
	}

	set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
	set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
	set_ButtonLED_byPalette(RecBankButtonLED, GREEN);
	set_ButtonLED_byPalette(RecButtonLED, GREEN);
	display_all_ButtonLEDs();

	return true;
}

