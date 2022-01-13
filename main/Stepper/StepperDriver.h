/**
 * StepperDriver.h
 *
 *  Created on: Jan 12, 2022
 *      Author: doug
 *
 */

#ifndef MAIN_STEPPER_STEPPERDRIVER_H_
#define MAIN_STEPPER_STEPPERDRIVER_H_
#include "../Sequencer/DeviceDef.h"
#include "StepperMotorController.h"
#include "esp_timer.h"
#include "esp_log.h"


#define EVENT_STEPPER_SET_HOME 101
#define EVENT_STEPPER_SET_LOWER_LIMIT 102
#define	EVENT_STEPPER_SET_UPPER_LIMIT 103
#define EVENT_STEPPER_SET_RAMP        104
#define EVENT_STEPPER_GOHOME          105
#define	EVENT_STEPPER_GOUPPER         106
#define EVENT_STEPPER_GOLOWER         107
#define EVENT_STEPPER_ESTOP           108
#define EVENT_STEPPER_ENABLE          109
#define EVENT_STEPPER_DISABLE         110

#define EVENT_STEPPER_GET_ABS_POS     111
#define EVENT_STEPPER_GET_REL_POS     112
#define EVENT_STEPPER_GET_LOW_LIMIT   113
#define EVENT_STEPPER_GET_UPR_LIMIT   114
#define EVENT_STEPPER_GET_REM_TIME    115

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
	void handleNodCommands(const Message *msg);
	void handleRotCommands(const Message *msg);

};

#endif /* MAIN_STEPPER_STEPPERDRIVER_H_ */
