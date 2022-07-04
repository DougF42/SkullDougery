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
#include "Arduino.h"

#include "../config.h"
#include "../Sequencer/SwitchBoard.h"
#include "../Sequencer/DeviceDef.h"
#include "../Sequencer/Message.h"

// The rate (in uSeconds) that 'run' will be called.
#define CLOCK_RATE 500

static const char *TAG="STEPPER DRIVER::";

esp_timer_handle_t  StepperDriver::myTimer=nullptr;

StepperDriver::StepperDriver (const char *name) :DeviceDef(name)
{
	nodControl=nullptr;
	rotControl=nullptr;
}

StepperDriver::~StepperDriver ()
{
	// Auto-generated destructor stub
}


/**
 * This is how messages (commands) are delivered to us.
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
		ESP_LOGD(TAG, "STEPPER TIME INTERVAL %ld", maxInterval );
		maxInterval = 0L;
	}

	else if (msg->event == EVENT_STEPPER_CONTROL_TIMER) {
		controlTimer(msg->value!=0);
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
 * Stop or start the timer, based on the 'flag'.
 * If the timer is already in that mode, do nothing.
 * @param flag - boolean. true to start, false to stop timer.
 *
 */
void StepperDriver::controlTimer(bool flag) {
	if (flag == timer_state) return;
	if (flag)
	{
		esp_timer_start_periodic(myTimer, CLOCK_RATE); // Interval in uSeconds.
		timer_state=true;
	}
	else
	{
		esp_timer_stop(myTimer);
		timer_state=false;
	}

}

/**
 *
 * Do next time step.
 *  This just calls 'run' for eacho of the running tasks, then
 *  looks at each one's 'next step' time and starts a one-shot
 *  timer for the shortest period.
 *
 * NOTE:
 *     We require that the on-shot timer has been stopped before entry.
 *     IF called from the clock callback, this is automatic.
 *     IF calling from the anywhere else, THAT function
 *        must ensure that the clock is stopped BEFORE calling doOneStep.
 *
 *     The clock is automatically started by this routine.
 *
 */
void StepperDriver::doOneStep()
{
	//nodControl->Dump();
	nodControl->Run();
	rotControl->Run();

	// NOTE: We must use micros here from arduino.h, because it
	//       truncates time down to unsigned long - as used by
	//       the Arduino library.
	unsigned long now = micros();

	unsigned long nextNodTime=nodControl->GetTimeToNextStep();
	nextNodTime = (nextNodTime > now)? nextNodTime - now: 2^63;

	unsigned long nextRotTime=rotControl->GetTimeToNextStep();
	nextRotTime = (nextRotTime > now)? nextRotTime - now: 2^63;

//	ESP_LOGD(TAG, "NOW is %lu.  NextStepAt: %ld   ROT Interval: %lu",
//			now, rotControl->GetTimeToNextStep(), nextRotTime);

	uint64_t delayForUsec=(nextNodTime < nextRotTime)? nextNodTime:nextRotTime;
	delayForUsec = (delayForUsec > 10000000LL) ? 10000000LL:delayForUsec;

	esp_timer_start_once(myTimer, delayForUsec);
}

/**
 * This is the 'StepperDriver' task.
 *
 * To quote a much-overused line - there can be only one instance
 * of this!
 *
 *  All stepper drivers are initialized from here, and handled
 *     internally.
 *
 * Apart from some intialization, it doesnt really
 * do anything but keep the task alive so that the
 * timer interrupt - the real workhorse - can do its thing.
 *
 */
void StepperDriver::runTask(void *param) {
	StepperDriver *me = (StepperDriver *)param;

	// initialize the controllers
	me->rotControl = new StepperMotorController(UNIPOLAR, ROTATE_PINA, ROTATE_PINB, ROTATE_PINC,ROTATE_PIND, STEPPER_NO_LED);
	SwitchBoard::registerDriver(TASK_NAME::ROTATE, me);
	ESP_LOGI(TAG, "ROTATE is registered");

	me->nodControl = new StepperMotorController(UNIPOLAR, NOD_PINA,    NOD_PINB,    NOD_PINC,   NOD_PIND,  STEPPER_NO_LED);
	// Register us for action
	SwitchBoard::registerDriver(TASK_NAME::NODD, me);
	ESP_LOGI(TAG, "NODD is registered");

	// Initialize the timer
	esp_timer_create_args_t timer_cfg={};
	timer_cfg.callback=&me->clockCallback;
	timer_cfg.arg = param;
	timer_cfg.name = "stepperTimer";
	timer_cfg.dispatch_method=ESP_TIMER_TASK;
	ESP_ERROR_CHECK(esp_timer_create(  &timer_cfg, &(me->myTimer)));

	me->doOneStep();   // First time thru - start the clock.

	//clockCallback(param);   // Force first time thru the clock callback.
	// esp_timer_start_once(me->myTimer, nextTime-now);
	//esp_timer_start_periodic(me->myTimer, 1000); // Interval in uSeconds.
	me->controlTimer(true);

	// Sit and twiddle our thumbs while the timer does all the work
	while(true)
	{
		vTaskDelay(10000);
	}

}
