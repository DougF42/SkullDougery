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
#include "esp_task_wdt.h"

#include "../config.h"
#include "../Sequencer/SwitchBoard.h"
#include "../Sequencer/DeviceDef.h"
#include "../Sequencer/Message.h"
#include "../Parameters/RmNvs.h"



// If defined, then the  'doOneStep' method is always called
// at the MIN_CLOCK_RATE value.
// Otherwise, each time thru 'doOneStep' we calculate the
// set the timer to that interval (never less
// than STEPPER_FIXED_CYCLE_TIME) before calling 'doOneStep' again
#define STEPPER_FIXED_CYCLE_TIME 1

// The minimum rate (in uSeconds) that 'run' will be called, or
// the actual rate that 'run' is called (in microseconds)
#define MIN_CLOCK_RATE 100

static const char *TAG="STEPPER DRIVER::";

StepperDriver::StepperDriver (const char *name) :DeviceDef(name)
{
#ifdef USE_TIMER
  myTimer    = nullptr;
  timerState = false;
#endif
  nodControl = nullptr;
  rotControl = nullptr;
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
 *
 * We accept the following message types:
 *     EVENT_STEPPER_EXECUTE_CMD   101
 */
void StepperDriver::callBack (const Message *msg)
{
	Message *resp;
	controlTimer(-1);
	StepperMotorController *target = nullptr;

	if (msg->event == EVENT_STEPPER_EXECUTE_CMD) {
		// pic target - NOD or ROT ???
		switch (msg->destination) {
		case (TASK_NAME::NODD):
			target = nodControl;
			break;

		case (TASK_NAME::ROTATE):
			target = rotControl;
			break;

		default: // Should not happen!
			resp = Message::create_message(msg->response, msg->destination,
					msg->event, 0, 0, "Bad format, or unknown device !");
			break;
		}

		// RUN the command on the target, return a appropriate response
		const char *res = target->ExecuteCommand(msg->text);

		ESP_LOGD(TAG, "Command:'%s'   response: '%s'", msg->text, res);

		if ((res == nullptr) || (res[0] == '\0')) {   // no resp text means "OK"
			resp = Message::create_message(msg->response, msg->destination,
					msg->event, 0, 0, "OK");
		} else { // Something to report to caller...
			resp = Message::create_message(msg->response, msg->destination,
					msg->event, 0, 0, res);
		}

		SwitchBoard::send(resp);
		doOneStep();
	}
}

#ifdef USE_TIMER
/**
 * This is the timer callback - each time thru
 * we call the 'run' function of each StepperMotorController.
 *
 *
 * In all cases, we expect the timer to be stopped upon entry.
 *
 */
void StepperDriver::clockCallback(void *arg) {
	StepperDriver *me= (StepperDriver *)arg;
	me->doOneStep();
}


/**
 * Stop or start the one-shot timer, based on the argument
 *    -1 stops the counter.
 *     0 sets a default value of MIN_CLOCK_RATE.
 *     >0 sets the indicated interval (in microseconds)
 *
 * If the timer is already in that mode, do nothing.
 * @param flag - if 0, stop the timer. Non-zero - set the
 *      one-shot timer for the indicated number of microseconds.
 *
 */
void StepperDriver::controlTimer(int64_t value) {
  ESP_LOGD(TAG, "controlTimer: arg is %lld", value);

  // To set a new interval, the timer MUST be stopped.
  esp_timer_stop(myTimer);

  if (value != 0)
    {  // set the requested delay period
      esp_timer_start_once(myTimer, value); // Interval in uSeconds.
    }
}


/**
 *
 * Do next time step.
 *  This just calls 'run' for each of the running tasks, then
 *  looks at each one's 'next step' time and starts a one-shot
 *  timer for the shortest period.
 *
 * NOTE:
 *     We require that the on-shot timer has been stopped before entry.
 *     IF called from the clock callback, this is automatic.
 *     IF calling from the anywhere else, THAT function
 *        must ensure that the clock is stopped BEFORE calling doOneStep.
 *
 *
 */
void StepperDriver::doOneStep()
{
	nodControl->Run();
	rotControl->Run();

	int64_t stepLength_usecs=1000;  // if not running, keep busy?

	if (nodControl->GetState() == RUNNING)
	{
		stepLength_usecs=nodControl->GetTimeToNextStep();
	}

#ifdef tempddd
	if (rotControl->GetState() == RUNNING)
	{

		int64_t nxtRotTime = stepLength_usecs = rotControl->GetTimeToNextStep();
		stepLength_usecs = (stepLength_usecs < nxtRotTime)? timeOfNextPass: nxtRotTime;
	}
#endif

	controlTimer(stepLength_usecs);
}
#endif

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
	char cmd[64];
	const char *resp;

	// initialize the controllers
	me->rotControl = new StepperMotorController(UNIPOLAR, ROTATE_PINA, ROTATE_PINB, ROTATE_PINC,ROTATE_PIND, STEPPER_NO_LED);
	SwitchBoard::registerDriver(TASK_NAME::ROTATE, me);
	ESP_LOGI(TAG, "ROTATE is registered");
	resp=me->rotControl->ExecuteCommand("EN");
	ESP_LOGD(TAG,"Rotate EN command resp=%s",resp);

	resp=me->rotControl->ExecuteCommand("SH");

	sprintf(cmd, "SL%d",RmNvs::get_int(RMNVS_ROT_MIN_POS));
	resp=me->rotControl->ExecuteCommand(cmd);
	ESP_LOGD(TAG,"Roate %s command. Resp=%s",cmd, resp);

	sprintf(cmd, "SU%d", RmNvs::get_int(RMNVS_ROT_MAX_POS));
	resp=me->rotControl->ExecuteCommand(cmd);
	ESP_LOGD(TAG, "Rotate: %s command. Resp=%s",cmd,resp);

	me->nodControl = new StepperMotorController(UNIPOLAR, NOD_PINA,    NOD_PINB,    NOD_PINC,   NOD_PIND,  STEPPER_NO_LED);
	// Register us for action
	SwitchBoard::registerDriver(TASK_NAME::NODD, me);
	ESP_LOGI(TAG, "NODD is registered");

	me->nodControl->ExecuteCommand("EN");
	me->nodControl->ExecuteCommand("SH");

	sprintf(cmd, "SL%d", RmNvs::get_int(RMNVS_NOD_MIN_POS));
	me->nodControl->ExecuteCommand(cmd);

	sprintf(cmd, "SU%d", RmNvs::get_int(RMNVS_NOD_MAX_POS));
	me->nodControl->ExecuteCommand(cmd);

#ifdef USE_TIMER
	// Initialize the timer
	esp_timer_create_args_t timer_cfg={};
	timer_cfg.callback=&me->clockCallback;
	timer_cfg.arg = param;
	timer_cfg.name = "stepperTimer";
	timer_cfg.dispatch_method=ESP_TIMER_TASK;
	ESP_ERROR_CHECK(esp_timer_create(  &timer_cfg, &(me->myTimer)));

	me->controlTimer(MIN_CLOCK_RATE); // start the clock
#endif
	// Sit and twiddle our thumbs while the timer does all the work
	while(true)
	{
		vTaskDelay(20);

	}

}
