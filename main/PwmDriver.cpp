/*
 * PwmDriver.cpp
 *
 *  Created on: Sep 23, 2021
 *      Author: doug
 *
 *
 *      This is used to drive either an LED (fast) or sevo (slow)
 *      Servo wants 50cps,
 *      LES wants  faster - 400 cps?
 *
 *    This is a 'driver' that actually handles all of our PWM type needs:
 *        2 eyes, running 'fast'.
 *        Mount   runing on a small servo.
 *        Nod     runing on a small servo.
 *
 *     We use 2 'fast' pwm's, and 2 'slow' pwm's.  The fast PWM is clocked at
 *     LED_FREQ (see pwmdriver.h). The Slow timer is set for the
 *     SERVO_FREQ (see pwmdriver.h), which should be ideal for servo control.
 *
 *     The LEDC interface is used. While tempting, we do NOT attempt to
 *     implement the 'fade' features of the interface.
 *
 *     The LEDC interface is not thread safe, so we should not attempt to run
 *     any pwm outputs external to this driver.
 *
 *  This driver is not a separate task, the sets the pwm values immmediatly as
 *  part of the callback function. Yes, this breaks the rules, but setting the pwm
 *  should be a really fast action.
 *
 *     EVENTS:
 *     The driver defines one extra event, which can be applied to any of the pwm's:
 *
 *     PWM_EVENT_SET - This sets the output pulse width to a 'value' scaled from 0 to 100
 *      (i.e.: A percentage).
 *
 */
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/ledc.h"

#include "esp_err.h"
#include "sdkconfig.h"
#include "esp_log.h"

#include "config.h"
#include "PwmDriver.h"
#include "Sequencer/DeviceDef.h"
#include "Sequencer/Message.h"
#include "Sequencer/SwitchBoard.h"
#include "Interpolate.h"

#define INCLUDE_FAST
#define INCLUDE_SERVO

static const char *TAG = "PWMDRIVER:";

#define LED_TIMER     LEDC_TIMER_0
#define SERVO_TIMER   LEDC_TIMER_1

#include <string.h>
#include "PwmDriver.h"

bool PwmDriver::alreadyInited = false;

PwmDriver::PwmDriver (const char *name) :
		DeviceDef (name )
{
	ESP_LOGD(TAG, "In PWMDRIVER init..." );
	devName = strdup ("PWMDRIVER" );
	if (alreadyInited) return;
	alreadyInited = true;

#ifdef INCLUDE_FAST
	CH_LEFT_EYE = LEDC_CHANNEL_0;
	CH_RIGHT_EYE = LEDC_CHANNEL_1;
#endif
#ifdef INCLUDE_SERVO
	CH_JAW      = LEDC_CHANNEL_2;
#endif
	timerSetup ();
	ESP_LOGD(TAG, "Passed fast timer setup" );

	// Register with Sequencer
#ifdef INCLUDE_FAST
	ESP_LOGD(TAG, "Register EYES and EYEDIR");
	SwitchBoard::registerDriver (TASK_NAME::EYES, this );
	maxLedDuty = std::floor (((1 << LED_DUTY_RES_BITS) - 1) );
	ESP_LOGI(TAG, "...Calculated maxLedDuty factor: %d ", maxLedDuty );
	ESP_LOGI(TAG, "");

#endif
#ifdef INCLUDE_SERVO

	ESP_LOGD(TAG, "PwmDriver initialization - CH_JAW is %d", CH_JAW);
	maxServoDuty = std::floor(((1<<SERVO_DUTY_RES_BITS)-1) );
	SwitchBoard::registerDriver (TASK_NAME::JAW, this);
	ESP_LOGI(TAG, "Calculated maxServoDuty factor: %d ", maxServoDuty);

	servo_min =.0005/.020 * maxServoDuty; // 1 millisec out of 20
	servo_max =.0025/.020 * maxServoDuty; // 2 millisec out of 20
	ESP_LOGI(TAG, "SERVO setting range for 1 to 2 millisec is %d to %d", servo_min, servo_max);

	// SET UP INTERP TABLE FOR JAW
	interpJaw.AddToTable(0, servo_min);
	interpJaw.AddToTable(100,servo_max);
#endif
}

PwmDriver::~PwmDriver ()
{
#ifdef INCLUDE_FAST
	SwitchBoard::deRegisterDriver (TASK_NAME::EYES );
#endif
#ifdef INCLUDE_SERVO
	SwitchBoard::deRegisterDriver (TASK_NAME::JAW);
#endif
	free (devName );
	devName = nullptr;
}


/**
 * Called when we need to change a device.
 * EVENTS are defined in the header.
 *
 * NOTE: ALL magnitudes are in terms of 0...100%
 *   'EYES' move both eyes.
 *
 *   'EYEDIR' determines the difference in brightness
 *         between the lights
 *
 */
void PwmDriver::callBack (const Message *msg)
{
	switch (msg->event)
	{
		case (EVENT_ACTION_SETVALUE):
			// Set the duty cycle. We do this immediately - I
			// expect the ledc interface to be fast enough for this to
			// be okay.

#ifdef INCLUDE_FAST
			if (msg->destination == TASK_NAME::EYES)
			{
				if (msg->event == EVENT_ACTION_SETVALUE) {
	//			ESP_LOGD(TAG,
	//					"PWMDRIVER Callback: Set EYES to %d. Actual value will be %d",
	//					msg->value, duty );
				// TODO: Factor in EYEDIR
				ledc_set_duty (LEDC_HIGH_SPEED_MODE, CH_RIGHT_EYE, msg->value );
				ledc_set_duty (LEDC_HIGH_SPEED_MODE, CH_LEFT_EYE, msg->rate);
				ledc_update_duty (LEDC_HIGH_SPEED_MODE, CH_RIGHT_EYE );
				ledc_update_duty (LEDC_HIGH_SPEED_MODE, CH_LEFT_EYE );
				}
				if (msg->event == EVENT_ACTION_SETDIR) {
					// TODO: NOT implemented.
				}
			}


#endif
#ifdef INCLUDE_SERVO
			if (msg->destination == TASK_NAME::JAW)
			{
				int duty=msg->value;
				// Limit DUTY and scale it so that 0=0deg (no sig) and 100=180Deg (5%)
				if (duty<0) duty=0;
				else if (duty>100) duty=100;

				duty = interpJaw.interp(duty);
				ESP_LOGI(TAG, "PWMDRIVER Callback*: Set MOUTH to %d. Actual value will be %d",
						msg->value, duty);

				ledc_set_duty (LEDC_LOW_SPEED_MODE, CH_JAW, duty);
				ledc_update_duty (LEDC_LOW_SPEED_MODE, CH_JAW );
				break;
			}
#endif
			break;

		default:
			break;
	}  // End of switch
}

/**
 * We use both high-speed timers.
 *     Timer 0 is for the LED's, at 5000 hz
 *     Timer 1 is for the servos, at 50 hz.
 *
 * CHANNELS:
 *     Channel h_ch0 and ch_1 are the Left and Right Eye LED's
 *     Channel h_ch2 is for the Jaw.
 *     Channel h_ch3 is for the nod.
 *     *THE ROTATE STEPPER IS NOT DRIVEN BY THIS DRIVER!
 *
 * PINS:
 *     See Below
 */
void PwmDriver::timerSetup ()
{
	int duty = 50;

#ifdef INCLUDE_FAST
	// setup LED timer
	ledc_timer_config_t ledc_timer_fast;
	ledc_timer_fast.speed_mode = LEDC_HIGH_SPEED_MODE;   // timer mode
	ledc_timer_fast.duty_resolution = LED_DUTY_RES_BITS; // Duty Resolution ((2**13)-1)
	ledc_timer_fast.timer_num = LED_TIMER;               // timer index
	ledc_timer_fast.freq_hz = LED_FREQ;               // frequency of PWM signal
	ledc_timer_fast.clk_cfg = LEDC_AUTO_CLK;     // Auto select the source clock
	ESP_LOGD(TAG, "PWM SETUP: About to set up Fast Timer TIMER" );
	if (ESP_OK != ledc_timer_config (&ledc_timer_fast ))
	{
		ESP_LOGE(TAG, "Error: PWM SETUP FAST TIMER FAILED!" );
	}
	ESP_LOGD(TAG, "PWM SETUP: Fast Timer is Set" );

	// - - - - -
	// Configure LED channel - Left Eye
	ledc_channel_config_t ledc_conf_eyes;
	ledc_conf_eyes.channel = CH_LEFT_EYE;
	ledc_conf_eyes.duty = duty;
	ledc_conf_eyes.gpio_num = PIN_LEFT_EYE;
	ledc_conf_eyes.intr_type = LEDC_INTR_DISABLE;
	ledc_conf_eyes.speed_mode = LEDC_HIGH_SPEED_MODE;
	ledc_conf_eyes.timer_sel = LED_TIMER;
	ledc_conf_eyes.hpoint = 0;
	if (ESP_OK != ledc_channel_config (&ledc_conf_eyes ))
	{
		ESP_LOGE(TAG, "Error: LEDC config for left eye failed!" );
	}
	ESP_LOGD(TAG, "PWM SETUP: Left Eye Configured and set to 0" );
	ledc_set_duty (LEDC_HIGH_SPEED_MODE, CH_RIGHT_EYE, 0 );
	ledc_update_duty (LEDC_HIGH_SPEED_MODE, CH_RIGHT_EYE );

	// Configure LED channel - Right Eye
	ledc_conf_eyes.channel = CH_RIGHT_EYE;
	ledc_conf_eyes.duty = duty;
	ledc_conf_eyes.gpio_num = PIN_RIGHT_EYE;
	ledc_conf_eyes.intr_type = LEDC_INTR_DISABLE;
	ledc_conf_eyes.speed_mode = LEDC_HIGH_SPEED_MODE;
	ledc_conf_eyes.timer_sel = LED_TIMER;
	ledc_conf_eyes.hpoint = 0;
	if (ESP_OK != ledc_channel_config (&ledc_conf_eyes ))
	{
		ESP_LOGE(TAG, "Error: LEDC config for right eye failed" );
	}
	ledc_set_duty (LEDC_HIGH_SPEED_MODE, CH_RIGHT_EYE, 0 );
	ledc_update_duty (LEDC_HIGH_SPEED_MODE, CH_RIGHT_EYE );
#endif

#ifdef INCLUDE_SERVO
	// Configure SLOW timer
	ledc_timer_config_t ledc_timer_slow;
	ledc_timer_slow.speed_mode = LEDC_LOW_SPEED_MODE;    // timer mode
	ledc_timer_slow.timer_num = SERVO_TIMER;              // timer index
	ledc_timer_slow.freq_hz = SERVO_FREQ;                 // frequency of PWM signal
	ledc_timer_slow.duty_resolution = SERVO_DUTY_RES_BITS;// Duty Resolution ((2**13)-1)
	ledc_timer_slow.clk_cfg = LEDC_AUTO_CLK;              // Auto select the source clock
	if(ESP_OK!=ledc_timer_config(&ledc_timer_slow)) {
		ESP_LOGD(TAG, "ERROR: Set slow timer failed!");
	}
	ESP_LOGD(TAG, "PWM SETUP: Slow Timer is Set");

	// config Jaw bone channel
	ledc_channel_config_t ledc_conf_jaw;
	ledc_conf_jaw.gpio_num = PIN_JAW_SERVO;
	ledc_conf_jaw.speed_mode = LEDC_LOW_SPEED_MODE;
	ledc_conf_jaw.channel = CH_JAW;
	ledc_conf_jaw.intr_type  = LEDC_INTR_DISABLE;
	ledc_conf_jaw.timer_sel  = SERVO_TIMER;
	ledc_conf_jaw.duty  = duty;
	ledc_conf_jaw.hpoint   = 0;
	if (ESP_OK!=ledc_channel_config(&ledc_conf_jaw)) {
		ESP_LOGD(TAG, "ERROR: Config jaw failed!");
	}
	ESP_LOGD(TAG, "PWM SETUP: JAW servo channel Configured");

#endif
}
