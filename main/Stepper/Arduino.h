/**
 * Arduino.h
 *
 *  Created on: Jan 11, 2022
 *      Author: doug
 *
 * These functions exist in the 'Arduino' development environment,
 * but are not necessarily in the esp idf environment. They are
 * designed to have the same semantics as the Arduino library,
 * so they should not require any code changes.
 *
 * - - - -
 *
 * OTHER Changes to *.h and *.cpp files:
 * (0) My 'lint' program objects to trailing '\' in comments, so added a '.' to prevent this failure.
 *
 * (1) Initialize MaxVelocity, TargetOrSteps, TotalSteps, StepCount to 0 in
 *        constructor (these were not previously initialized).
 *
 * (2) IMPLEMENT NON-DIGITAL features:
 *    * pin defs are not constant.
 *    * Separate constructors for DIGITAL and NON-DIGITAL types.
 *         (also added new 'commmonInit' function to do initializations
 *         that are common to both).
 *    * reset only appropriate data lines for ESTOP.
 *    * do not set direction lines for NON-DIGITAL types in startRotation.
 *    * turn off all 4 pins when DISABLE called for NON-DIGITAL type.
 *    * do not change any pins when ENABLE is called for NON-DIGITAL type.
 *
 * (3) String class not present on ESP32:
 *    * comment out 'execute command'.
 *    * 'run' now returns a status 'RunReturn_t', rather than returning a string.
 *    * Change 'version' from string to 'static const char *'.
 *        later c++ versions don't allow string init in class declaration,
 *        so moved version definition into '*.cpp'
 *    * getVersion now retuns const char * to version string (not String type)
 *
 */

#ifndef MAIN_STEPPER_ARDUINO_H_
#define MAIN_STEPPER_ARDUINO_H_
#include <string>
//#define String std::string

#include <cmath>
// cmath provides 'abs'
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

enum IOMODE { INPUT=1, INPUT_PULLUP=3, OUTPUT=4 };
enum DIRECT { LOW=0, HIGH=1 };
/**
 * Set the mode for the pin.
 *
 * Mode = INPUT, OUTPUT, or INPUT_PULLUP
 */
void pinMode(int pin, enum IOMODE mode);

/**
 * Set the level of the pin. dir is HIGH or LOW
 */
void digitalWrite(int pin, bool dir);

/**
 * Returns the time since startup in microseconds.
 * (Note: ESP 32 can keep 64 bit time - we truncate
 *    upper 16 bits to fit unsigned long)
 */
unsigned long micros();   // returns time since startip in microseconds

/**
 * Delay for a given number of microseconds.
 * note: NOT a precision measurement.
 *     This can result in another task getting control,
 *     resulting in us not returning for a longer-than-requested
 *     period.
 */
void delayMicroseconds(unsigned int us);

/**
 * Delay for a given number of microseconds.
 * note: NOT a precision measurement.
 *     This can result in another task getting control,
 *     resulting in us not returning for a longer-than-requested
 *     period.
 */
void delay(unsigned long );    // delay for milliseconds

#endif /* MAIN_STEPPER_ARDUINO_H_ */
