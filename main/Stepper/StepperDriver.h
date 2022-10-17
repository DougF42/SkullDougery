/**
 * StepperDriver.h
 *
 *  Created on: Jan 12, 2022
 *      Author: doug
 *
 */

#ifndef MAIN_STEPPER_STEPPERDRIVER_H_
#define MAIN_STEPPER_STEPPERDRIVER_H_
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../Sequencer/DeviceDef.h"
#include "StepperMotorController.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/semphr.h"

#define USE_TIMER

#define EVENT_STEPPER_EXECUTE_CMD 101
#define EVENT_GET_TIME            102
#define EVENT_STEPPER_CONTROL_TIMER 103


class StepperDriver: public DeviceDef
{
public:
	StepperDriver (const char *name);
	virtual ~StepperDriver ();
	void callBack(const Message *msg);
	void controlTimer(int64_t flag);
  	static void runTask(void *param);

private:
	StepperMotorController *nodControl;
	StepperMotorController *rotControl;

	esp_timer_handle_t myTimer;
	bool timerState;
	static void clockCallback(void *_me);
	void doOneStep();
};

#endif /* MAIN_STEPPER_STEPPERDRIVER_H_ */
