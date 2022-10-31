/*
 * @brief  - common configuration.
 *  NOTE: This should be included BEFORE any other includes,
 *       especially RTOS or IDF related includes.
 */
/**
 * Due to a change in how stuff is defined in recent compilers.,
 * the following definitions are changed to their 'upgraded' versions:
 */
#ifdef SKIPME
#include <stdint.h>

#undef int32_t
typedef __int32_t int32_t ;
#undef uint32_t
typedef __uint32_t uint32_t ;
#endif
#include <freertos/FreeRTOS.h>
#include <climits>

// This a 'random' number that we use to indicate
// that a integer conversion failed.
#define BAD_NUMBER 0xFFFFEFE

#define RESET_SWITCH GPIO_NUM_5

// WIFI/NETWORK SETTINGS
#define SKULL_WIFI_AUTHMODE WIFI_AUTH_WPA_WPA2_PSK

// speaker settings for driving I2S amplifier
#define I2S_SPEAKER_SERIAL_CLOCK      GPIO_NUM_19
#define I2S_SPEAKER_LEFT_RIGHT_CLOCK  GPIO_NUM_21
#define I2S_SPEAKER_SERIAL_DATA       GPIO_NUM_18
// volume control - (not currently implemented)
//#define VOLUME_CONTROL ADC1_CHANNEL_7


// button - GPIO 0 is the built in button on most dev boards
#define GPIO_BUTTON GPIO_NUM_0


/* = = = = = = = = = == */

// CORE assignments.
// thew CORE where the switchboard and most drivers reside
#define ASSIGN_SWITCHBOARD_CORE 1
#define ASSIGN_MUSIC_CORE     0
#define ASSIGN_STEPPER_CORE   1

#define SEQUENCER_PANIC_BACKLOG 5


/**
 * What file will we read from the FLASH?
 *  Note that directory name IS '/fs'.
 */
#define SOURCE_FILE_NAME "/fs/DaysMono.mp3"

// PIN Definitions
#define ESP_LED_PIN 2

// NOD - stepper
#define NOD_PINA GPIO_NUM_23
#define NOD_PINB GPIO_NUM_22
#define NOD_PINC GPIO_NUM_32
#define NOD_PIND GPIO_NUM_33

// ROTATE - stepper
#define ROTATE_PINA GPIO_NUM_17
#define ROTATE_PINB GPIO_NUM_16
#define ROTATE_PINC GPIO_NUM_4
#define ROTATE_PIND GPIO_NUM_15

// Paramters for the LED driver.
#define PIN_LEFT_EYE     27
#define PIN_RIGHT_EYE    14
#define LED_FREQ         500
#define LED_DUTY_RES_BITS    LEDC_TIMER_13_BIT
#define EYE_AVG_SIZE      400

// Parameters for the SERVO driver (Jaw - pwm)
// Freq in millisecs.
#define PIN_JAW_SERVO    13
#define SERVO_FREQ       50
#define SERVO_DUTY_RES_BITS  LEDC_TIMER_13_BIT
#define JAW_AVG_SIZE     400

/*
 * MAP - MACRO to re-map a number from one range to another.
 * @param x -    the number to be re-maped.
 * @param in_min - the lowest value for x
 * @param in_max - the highest value for x
 * @param out_min - the lowest output value.
 * @param out_max - the highest output value
 * @return  - the remapped value.
 */
#define map(_xx, _in_min, _in_max, _out_min, _out_max) ( (_xx - _in_min) * (_out_max - _out_min) / (_in_max - _in_min) + _out_min)
