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


#define EVENT_STEPPER_EXECUTE_CMD 101
#define EVENT_GET_TIME            102
#define EVENT_STEPPER_CONTROL_TIMER 103


class StepperDriver: public DeviceDef
{
public:
	StepperDriver (const char *name);
	virtual ~StepperDriver ();
	void callBack(const Message *msg);
	static void runTask(void *param);
	void controlTimer(bool flag);

private:
	StepperMotorController *nodControl;
	StepperMotorController *rotControl;
	static esp_timer_handle_t myTimer;

	static void clockCallback(void *_me);
	void doOneStep();
	bool timer_state;
};

#endif /* MAIN_STEPPER_STEPPERDRIVER_H_ */
