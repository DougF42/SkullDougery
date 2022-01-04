#include <freertos/FreeRTOS.h>
#include <driver/i2s.h>
#include <climits>

// This a 'random' number that we use to indicate
// that a integer conversion failed.
#define BAD_NUMBER 0xFFFFEFE

#define RESET_SWITCH GPIO_NUM_33
// WIFI/NETWORK SETTINGS
#define SKULL_WIFI_CHANNEL 1
#define SKULL_WIFI_SSID "Skulldougery"
#define SKULL_WIFI_PASS "password"
#define SKULL_WIFI_AUTHMODE WIFI_AUTH_WPA_WPA2_PSK
#define SKULL_WIFI_PORT  100



// comment this out if you want to use the internal DAC
//#define USE_I2S

// speaker settings - if using I2S
#define I2S_SPEAKER_SERIAL_CLOCK GPIO_NUM_19
#define I2S_SPEAKER_LEFT_RIGHT_CLOCK GPIO_NUM_21
#define I2S_SPEAKER_SERIAL_DATA GPIO_NUM_18
// #define I2S_SPEAKDER_SD_PIN GPIO_NUM_5

// For the ANALOG output
// Which dac channel? channel 1 is pin 25, 2 is pin 26.
#define USE_DAC_CHANNEL DAC_CHANNEL_1
// i2s speaker pins definition
extern i2s_pin_config_t i2s_speaker_pins;

// volume control - if required
//#define VOLUME_CONTROL ADC1_CHANNEL_7

// button - GPIO 0 is the built in button on most dev boards
#define GPIO_BUTTON GPIO_NUM_0



/* = = = = = = = = = == */

// CORE assignments.
// thew CORE where the switchboard and most drivers reside
#define ASSIGN_SWITCHBOARD_CORE 0

// Where we send data to DAC
#define ASSIGN_MUSIC_CORE     0

// Where we do physical reads from FLASH
#define ASSIGN_FILEREAD_CORE  1

#define ASSIGN_MUSIC_TIMER
#define SEQUENCER_PANIC_BACKLOG 5


/**
 * What file will we read from the FLASH?
 *  Note that directory name IS '/fs'.
 */
#define SOURCE_FILE_NAME "/fs/DaysMono.mp3"

// PIN Definitions

// NOD - stepper
#define NOD_PINA GPIO_NUM_23
#define NOD_PINB GPIO_NUM_26
#define NOD_PINC GPIO_NUM_21
#define NOD_PIND GPIO_NUM_19

// ROTATE - stepper
#define ROTATE_PINA GPIO_NUM_18
#define ROTATE_PINB GPIO_NUM_17
#define ROTATE_PINC GPIO_NUM_16
#define ROTATE_PIND GPIO_NUM_4

// Paramters for the LED driver.
#define PIN_LEFT_EYE     27
#define PIN_RIGHT_EYE    14
#define LED_FREQ         500
#define LED_DUTY_RES_BITS    LEDC_TIMER_13_BIT
#define EYE_AVG_SIZE 256

// Parameters for the SERVO driver (Jaw - pwm)
// Freq in millisecs.
#define SERVO_FREQ           50
#define SERVO_DUTY_RES_BITS  LEDC_TIMER_13_BIT
#define PIN_JAW_SERVO    13
#define JAW_AVG_SIZE 1024

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
