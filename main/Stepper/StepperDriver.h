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


class StepperDriver: public DeviceDef
{
public:
	StepperDriver (const char *name);
	virtual ~StepperDriver ();
	void callBack(const Message *msg);
	static void runTask(void *param);

private:
	StepperMotorController *nodControl;
	StepperMotorController *rotControl;
	esp_timer_handle_t myTimer;

	static void clockCallback(void *_me);
	void doOneStep();

};

#endif /* MAIN_STEPPER_STEPPERDRIVER_H_ */
