# Stereo Triggered Sampler - Firmwares

### How do I know what version I have?
Using a computer:

  * Put your microSD card into your computer. Open up the `__Sample List__.html` file. At the top it will say "Firmware version XXX".

Without a computer:

  1) Go into System Mode (_hold both Reverse, both Play, REC and REC Bank buttons down for a few seconds_). 

  2) Press and hold the Edit button. Look at the colors of Reverse buttons:

*  Left Reverse = __White__, Right Reverse = __Orange__ ==> 1.3 (latest version)

*  Left Reverse = __White__, Right Reverse = __Red__ ==> 1.2 

<!--
*Key: The left Reverse button shows the Major version number, and the right Reverse button shows the Minor version number. Off = 0, White = 1, Red = 2, Orange = 3, Yellow = 4, Green = 5, Cyan = 6, Blue = 7, Magenta = 8, Lavender = 9* -->


*  Left Reverse = __Orange__, Right Reverse = __Orange__ ==> 1.0 or 1.1 (see note below)

3) If the Reverse buttons stay Orange, then you have v1.0 or 1.1. To tell the difference between v1.0 and v1.1, press and hold just the left Reverse button in System Mode. If both Reverse buttons turn White, then you have v1.1. If they remain Orange, you have v1.0.



###Change log:

----
#### v1.3

*Released:* (beta released Sept 26, 2017)

*Download:* [Firmware v1.3 WAV file -- BETA version](http://4mscompany.com/STS/firmware/STS-firmware-v1_3RC.wav)

*New Features:*

  * Drag-and-dropping WAV files to SD card works better:
    * Adding WAV files to an existing folder now adds the files to the bank if there are empty slots.
    * If there are no empty slots, the files can be added using Edit + Next File.
    * On boot, the STS will try to fill empty slots in banks by searching in the bank's folder:
      * If the bank contains files from multiple folders, the lowest-numbered sample file's folder is used. 

  * Holding down Bank 1 + Bank 2 + Edit while tapping REC and REC Bank adjusts 1V/oct cv jack offset.
    * _This compensates for small DC offsets on the jack which causes the two playback channels to be out of tune with each other and/or the Record channel (even if the Pitch knob is centered). All units are calibrated in the factory, but variations in power supplies and grounding configurations of cases may cause DC offset to appear in a user's system._
    * REC shifts it downwards, REC Bank shifts it upwards.
    * Settings are saved in flash (save using Edit + Save after making necessary adjustments).
    
*Fixes:*

  * Creating a folder with an exact color name in all lowercase added the folder twice. Fixed.
  * Edit+Next File while in an empty slot now searches in the folder of the first filled slot.

----
#### v1.2

*Released:* (released Sept 8, 2017)

*Download:* [Firmware v1.2 WAV file](http://4mscompany.com/STS/firmware/STS-firmware-v1_2RC.wav)

*New Features:*

  * Quantize 1V/oct jack added to System Mode. Each channel can be on/off seperately. Quantization is to semitones (12 notes per octave).
    * In System Mode, tap Bank button for either channel to toggle state.
    * Blue = On (jack is quantized to semitones)
    * Orange = Off (jack is not quantized)
    * Only effects 1V/oct jack, not the Pitch knob
    * Setting is saved in settings file on microSD card
    
  * Holding Edit+Rec+RecBank resets tracking compensation values to 1.0000
    * Updating to v1.2 firmware automatically sets tracking to 1.0000 the first time it is loaded (this is necessary because tracking is calculated differently in v1.2)

  * Auto Stop On Sample Change:  "Always keep playing" mode added. There are now three Auto Stop modes. The modes determine what happens when the sample is changed while a sample is being played.
    * Red = Always Stop: The sample instantly stops playing. If looping is on, the new sample starts playing immediately.
    * Blue = Change When Looping:  The sample keeps playing normally -- unless it's looping, in which case it stops immediately and the new sample starts.
    * Green = Always Keep Playing: The sample keeps playing normally. If it's looping then the new sample begins when the previous sample reaches the end.

*Fixes:*

  * 1V/octave tracking is tighter, +/-0.4 cents over C0-C3
 
*Changes:*

  * Faster entry into System Mode (2 seconds, instead of 4)
  * Pitch knob's plateau around the center detent is larger, and gradually slopes away from center to make it easier to tune to other sources


----
#### v1.1

*Released:* August 28, 2017

*Download:* [Firmware v1.1 WAV file](http://4mscompany.com/STS/firmware/STS-firmware-v1_1.wav)

*Changes:*

  * On boot, color of lights white index file is loading shows minor version number
  * In System Mode, holding down Reverse displays firmware version on Reverse buttons (White, White = 1.1)
  
*Fixes:*

  * Tapping Reverse after changing Channel Volume prematurely updated the Start Pos setting. Fixed.
  * Scrub End setting for samples was not loaded after reboot until Edit button was pushed. Fixed.

----
#### v1.0

Released: August 10, 2017

*Initial Release*

----
  

#### Features for future firmware versions:
  * System Mode setting to disable envelope when Length < 50%
  * System Mode setting to disable fade up/down when looping a sample
  * Allow for internally patching OUTs to INs for self-recording


