/*
 * PwmDriver.h
 *
 *  Created on: Sep 23, 2021
 *      Author: doug
 *
 *      This is used to drive either an LED (fast) or sevo (slow)
 *      Servo wants 50cps,
 *      LES wants  faster - 400 cps?
 */

#ifndef MAIN_PWMDRIVER_H_
#define MAIN_PWMDRIVER_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include "Sequencer/DeviceDef.h"
#include "Interpolate.h"
#define EVENT_ACTION_SETLEFT  100
#define EVENT_ACTION_SETRIGHT 101
class PwmDriver : DeviceDef{
public:
	PwmDriver(const char *name);
	virtual ~PwmDriver();
	void setDutyCycle(ledc_channel_t channel, int duty) ;
	ledc_channel_t	ch_Left_eye;  // assigned during init
	ledc_channel_t	ch_right_eye; // assigned during init
	ledc_channel_t	ch_jaw;
	void callBack(const Message *msg);
	inline int getMaxLedDuty()   {return (maxLedDuty);}
	inline int getMaxServoDuty() {return (maxServoDuty); }
	char *devName;

private:
	int maxLedDuty;
	int maxServoDuty;
	int servo_min;
	int servo_max;
	void timerSetup();
	static bool alreadyInited;
	Interpolate interpJaw;   // used to interpolate input value to actual setting for jaw
	Interpolate interpEyes;  // used to interpolate input value to actual setting for eyes
};

#endif /* MAIN_PWMDRIVER_H_ */
