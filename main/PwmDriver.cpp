/*
 * PwmDriver.cpp
 *
 *  Created on: Sep 23, 2021
 *      Author: doug
 *
 *
 *     This is used to drive either an LED (fast) or sevo (slow)
 *     We expect 2 eyes (Left & Right) on the 'fast' LED clock (500 hz),
 *     and 2 servos (Jaw and Nod) in the 'slow' LED clock (50cps)
 *
 *     The LEDC interface is used. While tempting, we do NOT attempt to
 *     implement the 'fade' features of the interface.
 *
 *     The LEDC interface is not thread safe, so we should not attempt to run
 *     any pwm outputs external to this driver.
 *
 *	   This driver is not a separate task, seting the pwm values occurs immediately as
 *     part of the callback function.  Yes,this bends the switchboard design rules, but
 *     setting the pwm is a  really fast action.
 *
 *     The driver registers itself as multiple devices:
 *       JAW
 *       EYES
 *
 *      ACTION_SETVALUE is applicable to ANY of these devices (both eyes are set equally).
 *      The range of action is normalized to 0 thru 1024.
 *
 *      In Addition, the EVENT_ACTION_SETLEFT and EVENT_ACTION_SETRIGHT are valid for
 *      the 'EYES' device to set the individual eyes.
 *
 *
 *NOTE: The 'value' for ALL Inputs range 1 to range 0...1024.
 */
#include "config.h"
#include "PwmDriver.h"
#include <cmath>

#include "esp_err.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "driver/ledc.h"

#include "Sequencer/DeviceDef.h"
#include "Sequencer/Message.h"
#include "Sequencer/SwitchBoard.h"
#include "Interpolate.h"

#include <string.h>


#define INCLUDE_FAST
#define INCLUDE_SERVO

static const char *TAG = "PWMDRIVER:";

#define LED_TIMER     LEDC_TIMER_0
#define SERVO_TIMER   LEDC_TIMER_1

bool PwmDriver::alreadyInited = false;

// Initialize
PwmDriver::PwmDriver (const char *name) :
		DeviceDef (name )
{
	ESP_LOGD(TAG, "In PWMDRIVER init..." );
	devName = strdup ("PWMDRIVER" );
	if (alreadyInited) return;
	alreadyInited = true;

#ifdef INCLUDE_FAST
	ch_Left_eye  = LEDC_CHANNEL_0;
	ch_right_eye = LEDC_CHANNEL_1;
#endif

#ifdef INCLUDE_SERVO
	ch_jaw      = LEDC_CHANNEL_2;
#endif
	timerSetup ();
	ESP_LOGD(TAG, "Passed fast timer setup" );

	// Register with Sequencer
#ifdef INCLUDE_FAST
	ESP_LOGD(TAG, "Register EYES");
	SwitchBoard::registerDriver (TASK_NAME::EYES, this );
	maxLedDuty = std::floor (((1 << LED_DUTY_RES_BITS) - 1) );
	ESP_LOGI(TAG, "...Calculated maxLedDuty factor: %d ", maxLedDuty );
	ESP_LOGI(TAG, "");
	interpEyes.setLimitFlag();
	interpEyes.AddToTable(0, 0);
	interpEyes.AddToTable(1024, (1<<LED_DUTY_RES_BITS)-1);

	ESP_LOGI(TAG, "SERVO setting range for .7 to 2.5 millisec is %d to %d", servo_min, servo_max);

#endif

#ifdef INCLUDE_SERVO
	ESP_LOGD(TAG, "PwmDriver initialization - CH_JAW is %d", ch_jaw);
	maxServoDuty = std::floor(((1<<SERVO_DUTY_RES_BITS)-1) );
	SwitchBoard::registerDriver (TASK_NAME::JAW, this);
	ESP_LOGI(TAG, "Calculated maxServoDuty factor: %d ", maxServoDuty);

	// Our hardware sets PWM from 0 to maxServoDuty (about 8k).
	// Out clock is set for 50 CPS (20 millisec) PWM frequency.
	// Control width is nominally 1 to 2 millisecs (.001 to .002).
	// Our servo ranges 180 deg, but we only want 90 deg or so.
	servo_min =.0007/.020 * maxServoDuty; // .7 millisec out of 20 for 0 deg.
	servo_max =.0025/.020 * maxServoDuty; // 2.5 millisec out of 20 for 180 deg.

	// SET UP INTERP TABLE FOR JAW
	interpJaw.setLimitFlag();
	interpJaw.AddToTable(0, servo_min);
	interpJaw.AddToTable(1024,servo_max);  // 2000 is max value from audio - arbitrary.
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
 * Called by SWITCHBOARD to deliver a MSG
 *  when we need to change a device.
 *
 * NOTE: ALL magnitudes are in terms of 0...1024
 *   'EYES' adjust the eye settings:
 *
 *
 *   'EYEDIR' determines the difference in brightness
 *         between the lights
 *
 */
void PwmDriver::callBack(const Message *msg) {
	uint32_t duty = msg->value;

	if (msg->destination == TASK_NAME::EYES) {
		duty = interpEyes.interp(duty);

		ESP_LOGD(TAG,
				"PWMDRIVER Callback: Set EYE(s) to %ld. Actual value will be %d",
				msg->value, duty);

		switch (msg->event) {
		case (EVENT_ACTION_SETLEFT):
			ledc_set_duty(LEDC_HIGH_SPEED_MODE, ch_Left_eye, duty);
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, ch_Left_eye);
			break;

		case (EVENT_ACTION_SETRIGHT):
			ledc_set_duty(LEDC_HIGH_SPEED_MODE, ch_right_eye, duty);
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, ch_right_eye);
			break;

		case (EVENT_ACTION_SETVALUE):
			ledc_set_duty(LEDC_HIGH_SPEED_MODE, ch_right_eye, duty);
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, ch_right_eye);
			ledc_set_duty(LEDC_HIGH_SPEED_MODE, ch_Left_eye, duty);
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, ch_Left_eye);
			break;
		}
	}
#ifdef INCLUDE_SERVO
	else {
		if (msg->destination == TASK_NAME::JAW) {
			duty = interpJaw.interp(msg->value);
				ESP_LOGI(TAG,
						"PWMDRIVER Callback*: Set JAW to %ld. Actual value will be %d",
						msg->value, duty);

			ledc_set_duty(LEDC_LOW_SPEED_MODE, ch_jaw, duty);
			ledc_update_duty(LEDC_LOW_SPEED_MODE, ch_jaw);
		}
	}
#endif
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
	ledc_timer_fast.freq_hz = LED_FREQ;                  // frequency of PWM signal
	ledc_timer_fast.clk_cfg = LEDC_AUTO_CLK;             // Auto select the source clock
	ESP_LOGD(TAG, "PWM SETUP: About to set up Fast Timer TIMER" );
	if (ESP_OK != ledc_timer_config (&ledc_timer_fast ))
	{
		ESP_LOGE(TAG, "Error: PWM SETUP FAST TIMER FAILED!" );
	}
	ESP_LOGD(TAG, "PWM SETUP: Fast Timer is Set" );

	// - - - - -
	// Configure LED channel - Left Eye
	ledc_channel_config_t ledc_conf_eyes;
	ledc_conf_eyes.channel = ch_Left_eye;
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
	ledc_set_duty (LEDC_HIGH_SPEED_MODE, ch_right_eye, 0 );
	ledc_update_duty (LEDC_HIGH_SPEED_MODE, ch_right_eye );

	// Configure LED channel - Right Eye
	ledc_conf_eyes.channel = ch_right_eye;
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
	ledc_set_duty (LEDC_HIGH_SPEED_MODE, ch_right_eye, 0 );
	ledc_update_duty (LEDC_HIGH_SPEED_MODE, ch_right_eye );
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
	ledc_conf_jaw.channel = ch_jaw;
	ledc_conf_jaw.intr_type  = LEDC_INTR_DISABLE;
	ledc_conf_jaw.timer_sel  = SERVO_TIMER;
	ledc_conf_jaw.duty  = (1<<SERVO_DUTY_RES_BITS)/2; // Middle of range???
	ledc_conf_jaw.hpoint   = 0;
	if (ESP_OK!=ledc_channel_config(&ledc_conf_jaw)) {
		ESP_LOGD(TAG, "ERROR: Config jaw failed!");
	}
	ESP_LOGD(TAG, "PWM SETUP: JAW servo channel Configured");

#endif
}
