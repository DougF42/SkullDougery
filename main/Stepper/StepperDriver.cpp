/**
 * StepperDriver.cpp
 *  This driver both the NOD and ROTATE steppers
 *  Created on: Jan 12, 2022
 *      Author: doug
 *
 */
#include <ctype.h>

#include "StepperDriver.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "string.h"

#include "../config.h"
#include "../Sequencer/SwitchBoard.h"
#include "../Sequencer/DeviceDef.h"
#include "../Sequencer/Message.h"


static const char *TAG="STEPPER DRIVER::";

StepperDriver::StepperDriver (const char *name) :DeviceDef(name)
{
	myTimer=0;
	nodControl=nullptr;
	rotControl=nullptr;
}

StepperDriver::~StepperDriver ()
{
	// Auto-generated destructor stub
}


/**
 * This is how messages are delivered to us.
 *
 * If the message contains a command to a StepperMotorController,
 * we stop the timer, execute the command, then call the timer interrupt
 * ( which will re-start the timer).
 */
void StepperDriver::callBack (const Message *msg)
{
	esp_timer_stop (myTimer );

	StepperMotorController *target=nullptr;

	switch (msg->destination)
	{
		case (TASK_NAME::NODD):
			target=nodControl;
			break;

		case (TASK_NAME::ROTATE):
			target = rotControl;
			break;

		default:
			ESP_LOGE(TAG, "callback - panic - this cant happen - CALL THE STUPID PROGRAMMER");
			return;
	}

	const char *res=target->ExecuteCommand(msg->text);
	Message *resp;
	ESP_LOGD(TAG, "Command:'%s'   response: '%s'", msg->text, res);

	if ((res==nullptr) || (res[0] == '\0'))
	{   // no resp text means "OK"
		resp=Message::create_message(
			msg->response,
			msg->destination,
			msg->event, 0, 0, "OK");
	} else { // Something to report to caller...
		resp=Message::create_message(
			msg->response,
			msg->destination,
			msg->event, 0, 0, res);
	}

	SwitchBoard::send(resp);
	esp_timer_stop(myTimer);
	doOneStep();
}


/**
 * This is the timer callback - each time thru
 * we call the 'run' function of each StepperMotorController.
 *
 * We *could* define two separate callbacks (with different timers),
 * but being lazy one timer handles both.
 *
 * In all cases, we expect the timer to be stopped upon entry.
 * We will start it
 */
void StepperDriver::clockCallback(void *arg) {
	StepperDriver *me= (StepperDriver *)arg;
	me->doOneStep();
}


/**
 *
 * Do next time step.
 *
 * NOTE:
 *     We require that the clock has been stopped befgore entry.
 *     IF called from the clock callback, this is automatic.
 *     IF calling from the device message callback, THAT function
 *        must stop the clock BEFORE calling doOneStep.
 *
 *     The clock is automatically started by this routine.
 *
 */
void StepperDriver::doOneStep()
{
	ESP_LOGD(TAG, "DO ONE STEP...");
	nodControl->Run();
	rotControl->Run();
	uint64_t now = esp_timer_get_time();

	uint64_t nextNodTime=nodControl->GetTimeToNextStep() - now;
	ESP_LOGD(TAG, "NOW is %llu   timetonext NOD is %lu  nextNodTime=%llu",
			now, nodControl->GetTimeToNextStep(), nextNodTime);

	uint64_t nextRotTime=rotControl->GetTimeToNextStep() - now;
	uint64_t nextTime=(nextNodTime < nextRotTime)? nextNodTime:nextRotTime;
	nextTime = (nextTime > 5000000LL)?5000000LL:nextTime;
	ESP_LOGD(TAG,"NOW=%lld  nextNodTime=%lld  nextRotTime=%lld  nextTime=%lld",
			now, nextNodTime, nextRotTime, nextTime);

	esp_timer_start_once(myTimer, nextTime);
}

/**
 * This is the 'StepperDriver' task.
 * Apart from some intialization, it doesnt really
 * do anything but keep the task alive so that the
 * timer interrupt - the real workhorse - can do its thing.
 *
 */
void StepperDriver::runTask(void *param) {
	StepperDriver *me = (StepperDriver *)param;

	// initialize the controllers
	me->rotControl = new StepperMotorController(UNIPOLAR, ROTATE_PINA, ROTATE_PINB, ROTATE_PINC,ROTATE_PIND, 0);
	SwitchBoard::registerDriver(TASK_NAME::ROTATE, me);
	ESP_LOGI(TAG, "ROTATE is registered");

	me->nodControl = new StepperMotorController(UNIPOLAR, NOD_PINA,    NOD_PINB,    NOD_PINC,   NOD_PIND,    0);
	// Register us for action
	SwitchBoard::registerDriver(TASK_NAME::NODD, me);
	ESP_LOGI(TAG, "NODD is registered");


	// Initialize the timer
	esp_timer_create_args_t timer_cfg={};
	timer_cfg.callback=&me->clockCallback;
	timer_cfg.name = "stepperTimer";
	timer_cfg.arg = me;
	timer_cfg.dispatch_method=ESP_TIMER_TASK;
	ESP_ERROR_CHECK(esp_timer_create(  &timer_cfg, &(me->myTimer)));

	clockCallback(me);   // First time thru the clock callback.

	// Sit and twiddle our thumbs while the timer does all the work
	while(true)
	{
		vTaskDelay(10000);
	}

}
