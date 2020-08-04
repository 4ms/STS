# Stereo Triggered Sampler - Firmwares

### How do I know what version I have?
Using a computer:

  * Put your microSD card into your computer. Open up the `__Sample List__.html` file. At the top it will say "Firmware version XXX".

Without a computer:

  1) Go into System Mode (_hold both Reverse, both Play, REC and REC Bank buttons down for a few seconds_). 

  2) Press and hold the Edit button. Look at the colors of Reverse buttons:

*  Left Reverse = __White__, Right Reverse = __Yellow__ ==> 1.4 (latest version)
*  Left Reverse = __White__, Right Reverse = __Orange__ ==> 1.3
*  Left Reverse = __White__, Right Reverse = __Red__ ==> 1.2 
*  Left Reverse = __Orange__, Right Reverse = __Orange__ ==> 1.0 or 1.1 (see note below)

<!--
*Key: The left Reverse button shows the Major version number, and the right Reverse button shows the Minor version number. Off = 0, White = 1, Red = 2, Orange = 3, Yellow = 4, Green = 5, Cyan = 6, Blue = 7, Magenta = 8, Lavender = 9* -->

3) If the Reverse buttons stay Orange, then you have v1.0 or 1.1. To tell the difference between v1.0 and v1.1, press and hold just the left Reverse button in System Mode. If both Reverse buttons turn White, then you have v1.1. If they remain Orange, you have v1.0.



###Change log:

----
### v1.4g ###

*__Released:__* August 4, 2020

*__Download:__* [Firmware v1.4g](https://4mscompany.com/media/STS/firmware/STS-firmware-v1.4g.wav)

*__Bug Fix:__*

  * __Sequencing Start Pos Glitch Fixed__: Using a sequenced CV into Start Pos with gates/triggers into Play Trig would sometimes play the wrong start position if Length was < 50% and triggers were fired fast enough. Fixed.

----

Intermediate versions:

(1.4f released June 17, 2020. Improved performance of start-up sequence, and more types of "extended" wav file formats supported)

----

### v1.4e ###

*__Released:__* October 20, 2017

*__Download:__* (not available, please use v1.4g)

*__New Features:__*

  * __Reduced latency__ by pre-loading each sample file in a bank the first time each sample is played. 
     * Latency from trigger until audio output as low as 0.7ms (with Trigger Delay turned to 0, see below)
     * The first time a sample is played after the bank is changed, latency is typically 5ms (may be more depending on wav file's sampling rate and bit depth). Subsequent times the sample is triggered the latency is 0.7ms
  * __Variable "Trigger Delay"__: Edit + REC Sample knob 
    * Compensates for slew/lag when using a CV Sequencer with the Play Trig jack and the either the Sample CV or 1V/oct jack
    * This feature also allows for latency reduction compared to prior firmware versions, which had a built-in delay of about 14ms
    * After receiving a trigger on the Play jack, the STS will wait the specified delay period before reading the 1V/oct and Sample CV jacks.
    * To use: Press Edit and turn Rec Sample knob. The knob's numbers (1-10) correspond to a delay amount:
        * PCB v1.0a: ranges from 0ms delay (knob at 1) to 14.3ms delay (knob at 10) 
        * PCB v1.1: ranges from 0ms delay (knob at 1) to 1.9ms delay (knob at 8), and extra-long delays of 4.1ms (knob at 9) and 8.2ms (knob at 10)
    * _Note: If upgrading to v1.4 causes your sequencer and STS to not play well together, set the Trigger Delay to "8" or higher_ 
     

  * __Monitoring on/off per channel__: Left and right channels can monitor the inputs separately.
     * Pressing PLAY on one channel while monitoring is on will turn monitoring off for just that channel.
     * Only works in Mono mode.
     * Monitor LED blinks to indicate split monitoring.
     * _Typical use would be to patch Right OUT -> Left IN, then Left OUT -> mixer. Then record the right channel's playback._
    
  * __Start-up banks__: Can set the default bank to be loaded at start-up.
     * Hold down Edit + Bank 1 + Bank 2 + left PLAY (Save) for 1 second. Current bank selection will be saved as the default after power on. 
  * __Envelopes can be turned off__: Both percussive envelope and longer playback fade in/out envelope
     * Two types of envelopes can be turned on or off:
       * Percussive Envelopes: this is the decay-only envelope applied to playback when Length < 50% (actually an attack-only envelope when Reverse is on)
       * Fade In/Out envelope: this is a very short envelope applied to any sample that's played with Length > 50%. It reduces clicks when starting, stopping, or looping playback.
     * In System Mode, left channel Reverse button sets the envelope settings:
       * Orange: Both envelopes enabled (default)
       * Red: Percussive envelope disabled
       * Yellow: Fade In/Out envelopes disabled
       * Off: Both envelopes disabled
     * Turning off all envelopes is recommended when playing CV sample files (wav files with clocks/gates, sequencer CV, or slow LFO waveshapes, etc).
     * Turning off Percussive Envelopes is interesting when doing "Granular" patches (see User Manual for example patches)
     * Can toggle Percussive Envelope mode by turning left side Length to 0, and holding left side Reverse for 2 seconds: Reverse button will flash/flicker
     * Can toggle Fade In/Out Envelope mode by turning left side Length to 100%, and holding left side Reverse for 2 seconds: Reverse button will flash/flicker
     
*__Fixes:__*

  * After exiting System Mode, right side Length was set to 0, until the knob was moved. Fixed.

----
Intermediate versions: (1.4beta-1 released October 13, 2017)
(1.4beta-2 released October 17, 2017: changed Length behavior)
(1.4c fixed sticky right length knob when exiting system mode)
(1.4d fixed bugs that appeared in v1.4beta2) 
(1.4e fixed bugs that appeared in v1.3) 

----
#### v1.3

*Released:* (beta released Sept 26, 2017)

*Download:* [Firmware v1.3 WAV file](http://4mscompany.com/media/STS/firmware/STS-firmware-v1.3.zip)

*New Features:*
 
  * __Drag-and-dropping WAV files to SD card works better__:
    * Adding WAV files to an existing folder now adds the files to the bank if there are empty slots.
    * If there are no empty slots, the files can be added using Edit + Next File.
    * On boot, the STS will try to fill empty slots in banks by searching in the bank's folder:
      * If the bank contains files from multiple folders, the lowest-numbered sample file's folder is used. 

  * __Adjust 1V/OCT jack offset__
    * Hold down Bank 1 + Bank 2 + Edit while tapping either REC or REC Bank button.
      * REC shifts downwards, REC Bank shifts upwards.
    * Settings are saved in memory (save using Edit + Save after making necessary adjustments).
    * If there is a difference in pitch between the audio being monitored and playback, or a difference in playback pitch between channels, then this may need to be adjusted. All units are calibrated in the factory, but variations in power supplies and grounding configurations of cases may cause DC offset to appear in a user's system.
    
*Fixes:*

  * Creating a folder with an exact color name in all lowercase added the folder twice. Fixed.
  * Edit+Next File while in an empty slot now searches in the folder of the first filled slot.

----
#### v1.2

*Released:* (released Sept 8, 2017)

*Download:* [Firmware v1.2 WAV file](http://4mscompany.com/media/STS/firmware/STS-firmware-v1.2.zip)

*New Features:*

  * __Quantize 1V/oct jack__ added to System Mode. Each channel can be on/off seperately. Quantization is to semitones (12 notes per octave).
    * In System Mode, tap Bank button for either channel to toggle state.
    * Blue = On (jack is quantized to semitones)
    * Orange = Off (jack is not quantized)
    * Only effects 1V/oct jack, not the Pitch knob
    * Setting is saved in settings file on microSD card
    
  * __Reset tracking compensaion__ to 1.0000
    * Holding Edit+Rec+RecBank  
    * Updating from v1.0 or v1.1 to v1.2 or later automatically sets tracking to 1.0000 the first time it is loaded (this is necessary because tracking is calculated differently in v1.2 and later)

  * __Auto Stop When Sample Changes__:  "Always keep playing" mode added. There are now three Auto Stop modes. The modes determine what happens when the sample is changed while a sample is being played.
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

*Download:* [Firmware v1.1 WAV file](http://4mscompany.com/media/STS/firmware/STS-firmware-v1.1.zip)

*Changes:*

  * On boot, color of lights while index file is loading shows minor version number
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
  * Allow for internally patching OUTs to INs for self-recording
  * Support AIFF files
  * Write index file in background, for faster boot time and "Edit+Save" time



