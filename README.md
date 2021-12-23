# Skull Driver code.
#
We will use the following controls:
* 2 LEDS, one per light. _GPIO 34_ (left) and _GPIO 35_ (right)
* 1 Servo - for the jaw  _GPIO 13_
* 1 Stepper for the nod _GPIO1_, _GPIO3_, _GPIO21_, _GPIO19_
* 1 Stepper - to rotate the head. _GPIO18_, _GPIO17_, _GPIO16_, _GPIO14_
* 1 Audio output - _GPIO 25_. mono.
    Audio is driven by a pre-saved sound byte in flash.  Flash memory is limited, so
    the .wav file should be 8khz mono, 16 bit per sound byte. The audio output driver
    will try and adapt to whatever rate is supplied. Stereo is not currently supported.

**STEPPER**
The AccelStepper library from the Arduino development library is copied and 
modified for the ESP environment. The Digital I/O is modified for ESP32, this 
is configured as a 'driver' - all communication is done via messages to a single
callback routine, including timeouts. The 'run' routine is called from the 
callback only when the next event is expected. Upon completion, the 'time of
next step' is calculated, and a event is schedualed in the sequencer.

**LEDS**    Are Driven by the PWM module in the ESP32.

**SERVOS**  Driven by the PWM module in the ESP32 - 50 CPS (20 msec), varying
       duty cycle on command

Initial input will be a combination of
* .wav format audio source.
   (Assume audio goes to a separate speaker? Comes from on-board file? Can ESP32 drive a speaker?)
   Sync on 1st data point above a threashold.
   The eyes will respond to base notesjaw to overall volume.
   
* .cmd - a command file (Alt: command line) that manually controls rotation (and other operations)
      either at specified time stamps OR on commnad.




