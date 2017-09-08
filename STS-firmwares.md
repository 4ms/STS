# Stereo Triggered Sampler - Firmwares

### How do I know what version I have?
Using a computer:

  * Put your microSD card into your computer. Open up the `__Sample List__.html` file. At the top it will say "Firmware version XXX".

Without a computer:

  1) Go into System Mode (check the manual if don't know how!). 

  2) Press and hold the Edit button. If the colors of Reverse buttons change, you have a version 1.2 or later:

*  Left=White, Right=Red ==> 1.2 (latest version)

<!--
*Key: The left Reverse button shows the Major version number, and the right Reverse button shows the Minor version number. Off = 0, White = 1, Red = 2, Orange = 3, Yellow = 4, Green = 5, Cyan = 6, Blue = 7, Magenta = 8, Lavender = 9* -->


*  Orange | Orange ==> 1.0 or 1.1 (see note below)

3) If the Reverse buttons stay Orange, then you have v1.0 or 1.1. To tell the difference between v1.0 and v1.1, press and hold just the left Reverse button in System Mode. If both Reverse buttons turn White, then you have v1.1. If they remain Orange, you have v1.0.



###Change log:

----

#### v1.2

*Released:* (release candidate Sept 8, 2017)

*Download:* [Firmware v1.2 Release Candidate WAV file](http://4mscompany.com/STS/firmware/STS-firmware-v1_2RC.wav) <<--not final version, being tested!

*New Features:*

  * Quantize 1V/oct jack added to System Mode. Each channel can be on/off seperately. Quantization is to semitones (12 notes per octave).
    * In System Mode, tap Bank button for either channel to toggle state.
    * Blue = On (jack is quantized to semitones)
    * Orange = Off (jack is not quantized)
    * Only effects 1V/oct jack, not the Pitch knob
    * Setting is saved in settings file on microSD card
    
  * Holding Edit+Rec+RecBank resets tracking compensation values to 1.0000
    * Updating to v1.2 firmware automatically sets tracking to 1.0000 the first time it is loaded (this is necessary because tracking is calculated differently in v1.2)

  * Auto Stop On Sample Change:  "Always" mode added. There are now three Auto Stop modes. The modes determine what happens when the sample is changed while a sample is being played.
    * Always: The sample instantly stops playing. If looping is on, the new sample starts playing immediately.
    * Looping Only:  The sample keep playing normally -- unless it's looping, in which case it stops immediately and the new sample starts.
    * Off: The sample keeps playing normally. If it's looping then the new sample begins when the previous sample reaches the end.

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

#### Known Bugs:
  * Setting a folder name to an exact color name can sometimes cause it to load twice.
  

#### Features for future firmware versions:
  * Auto-loading empty slots from unused files in the folder, if a dominant folder exists (currently, the user has to press Edit+Next File to load a sample into an empty slot)
  * System Mode setting to disable envelope when Length < 50%
  * System Mode setting to disable fade up/down when looping a sample
  * Allow for internally patching OUTs to INs for self-recording


