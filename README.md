# Skull Driver code.
#
# Acknowledgements:
The code for reading the .mp3 file is based on a .mp3 player published under the name 'atomic14',
with the source at https://github.com/atomic14/esp32-play-mp3-demo. Most of the code in the 'audio'
subdirectory is his (as far as I know) with the exception of the 'minimp3.h' file, which is 
atributed to https://github.com/lieff/minimp3, which was explicitly placed in the public domain
by the author.  
I don't know the real names of these software engineers, but I am gratefull to them for their work.
(I will gladly add their actual names to this notice if I find out who they are, and want me to).

*** PINOUTS ***
We will use the following controls:
* 2 LEDS, one per light. _GPIO 34_ (left) and _GPIO 35_ (right)
* 1 Servo - for the jaw  _GPIO 13_
* 1 Stepper for the nod _GPIO1_, _GPIO3_, _GPIO21_, _GPIO19_
* 1 Stepper - to rotate the head. _GPIO18_, _GPIO17_, _GPIO16_, _GPIO14_
* 1 Audio output - _GPIO 25_ (main channel)  and _GPIO 26_(duplicate or other channel).
    Audio is driven by a pre-saved sound byte in flash.  Flash memory is limited, so
    we use .mp3 file, 8khz mono. The audio output driver will try and adapt to 
    whatever rate is supplied. Stereo may actually work...

**STEPPER**
The AccelStepper library from the Arduino development library is copied and 
modified for the ESP environment. The Digital I/O is modified for ESP32, this 
is configured as a 'driver' - all communication is done via messages to a single
callback routine, including timeouts. The 'run' routine is called from the 
callback only when the next event is expected. Upon completion, the 'time of
next step' is calculated, and a event is schedualed in the sequencer.

**LEDS**    Are Driven by the PWM module in the ESP32, using a 'fast' clock.

**SERVOS**  Driven by the PWM module in the ESP32, using a 'slow' clock - 
           50 CPS (20 msec), varying the duty cycle on comand.

Initial input will be an *.mp3 file.
* .mp3 format audio source, stored in flash memory.
  INPUT FORMAT: .mp3.   8khz, 16 byte data, mono to conserve space.

_NOTE: There is a bug in the I2S code on master that keeps us from working. 
As a result we are using IDF version V4.2.2 (branch: release/v4.2)._

_FUTURE: (Assume audio goes to a separate speaker? Comes from on-board file? Can ESP32 drive a speaker?)
   The jaw and eyes will respond to amplitude of each block of ?32? bytes_

