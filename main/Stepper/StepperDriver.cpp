/**
 * StepperDriver.cpp
 *  This driver both the NOD and ROTATE steppers
 *  Created on: Jan 12, 2022
 *      Author: doug
 *
 */

#include "StepperDriver.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../config.h"
#include "../Sequencer/SwitchBoard.h"
#include "../Sequencer/DeviceDef.h"

static const char *TAG="STEPPER DRIVER::";

#define STOPCLOCK  xSemaphoreTake(mylock, portMAX_DELAY);\
		esp_timer_stop (myTimer );

#define STARTCLOCK xSemaphoreGive(mylock);\
		clockCallback (this );

StepperDriver::StepperDriver (const char *name) :DeviceDef(name)
{
	myTimer=0;
	nodControl=nullptr;
	rotControl=nullptr;
	mylock = xSemaphoreCreateBinaryStatic(&mylocksBuffer);
}

StepperDriver::~StepperDriver ()
{
	// Auto-generated destructor stub
}

/**
 * This is how messages are delivered to us.
 *
 * This just does a 'triage', and calls the appropriate handler
 * for the given destination
 * If the message contains a command to a StepperMotorController,
 * we stop the timer, do the command, then call the timer interrupt
 * ( which will re-start the timer)
 */
void StepperDriver::callBack (const Message *msg)
{
	switch (msg->destination)
	{
		case (TASK_NAME::NODD):
			handleNodCommands (msg );
			break;

		case (TASK_NAME::ROTATE):
			handleRotCommands (msg );
			break;

		default:
			ESP_LOGE(TAG,
					"Unknown destination %d delivered to StepperDriver message callback!!",
					TASK_IDX(msg->destination ));
			break;
	}
}



void StepperDriver::handleNodCommands (const Message *msg)
{
	Message *respMsg;
	switch (msg->event)
	{
		case (EVENT_ACTION_SETVALUE):
			// This causes us to go to the given position, at the given rate.
			STOPCLOCK
			nodControl->RotateAbsolute (msg->value, msg->rate );
			STARTCLOCK
			break;

		case (EVENT_STEPPER_SET_HOME):
			STOPCLOCK
			nodControl->SetHomePosition ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_SET_LOWER_LIMIT):
			STOPCLOCK
			nodControl->SetLowerLimit (msg->value );
			STARTCLOCK
			break;

		case (EVENT_STEPPER_SET_UPPER_LIMIT):
			STOPCLOCK
			nodControl->SetUpperLimit (msg->value );
			STARTCLOCK
			break;

		case (EVENT_STEPPER_SET_RAMP):
			STOPCLOCK
			nodControl->SetRamp (msg->value );
			STARTCLOCK
			break;

		case (EVENT_STEPPER_GOHOME):
			STOPCLOCK
			nodControl->RotateToHome ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_GOUPPER):
			STOPCLOCK
			nodControl->RotateToUpperLimit ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_GOLOWER):
			STOPCLOCK
			nodControl->RotateToLowerLimit ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_ESTOP):
			STOPCLOCK
			nodControl->EStop ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_ENABLE):
			STOPCLOCK
			nodControl->Enable ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_DISABLE):
			STOPCLOCK
			nodControl->Disable ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_GET_ABS_POS):
			respMsg = Message::future_Message (msg->response, TASK_NAME::NODD,
			EVENT_STEPPER_GET_ABS_POS, nodControl->GetAbsolutePosition (), 0 );
			SwitchBoard::send (respMsg );
			break;

		case (EVENT_STEPPER_GET_REL_POS):
			respMsg = Message::future_Message (msg->response, TASK_NAME::NODD,
			EVENT_STEPPER_GET_REL_POS, nodControl->GetRelativePosition (), 0 );
			SwitchBoard::send (respMsg );
			break;

		case (EVENT_STEPPER_GET_LOW_LIMIT):
			respMsg = Message::future_Message (msg->response, TASK_NAME::NODD,
			EVENT_STEPPER_GET_LOW_LIMIT, nodControl->GetLowerLimit (), 0 );
			SwitchBoard::send (respMsg );
			break;

		case (EVENT_STEPPER_GET_UPR_LIMIT):
			respMsg = Message::future_Message (msg->response, TASK_NAME::NODD,
			EVENT_STEPPER_GET_UPR_LIMIT, nodControl->GetUpperLimit (), 0 );
			SwitchBoard::send (respMsg );
			break;

		case (EVENT_STEPPER_GET_REM_TIME):
			break;

	}
}



void StepperDriver::handleRotCommands (const Message *msg)
{
	Message *respMsg;
	switch (msg->event)
	{
		case (EVENT_ACTION_SETVALUE):
			// This causes us to go to the given position, at the given rate.
			STOPCLOCK
			rotControl->RotateAbsolute (msg->value, msg->rate );
			STARTCLOCK
			;
			break;

		case (EVENT_STEPPER_SET_HOME):
			STOPCLOCK
			rotControl->SetHomePosition ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_SET_LOWER_LIMIT):
			STOPCLOCK
			rotControl->SetLowerLimit (msg->value );
			STARTCLOCK
			break;

		case (EVENT_STEPPER_SET_UPPER_LIMIT):
			STOPCLOCK
			rotControl->SetUpperLimit (msg->value );
			STARTCLOCK
			break;

		case (EVENT_STEPPER_SET_RAMP):
			STOPCLOCK
			rotControl->SetRamp (msg->value );
			STARTCLOCK
			break;

		case (EVENT_STEPPER_GOHOME):
			STOPCLOCK
			rotControl->RotateToHome ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_GOUPPER):
			STOPCLOCK
			rotControl->RotateToUpperLimit ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_GOLOWER):
			STOPCLOCK
			rotControl->RotateToLowerLimit ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_ESTOP):
			STOPCLOCK
			rotControl->EStop ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_ENABLE):
			STOPCLOCK
			rotControl->Enable ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_DISABLE):
			STOPCLOCK
			rotControl->Disable ();
			STARTCLOCK
			break;

		case (EVENT_STEPPER_GET_ABS_POS):
			respMsg = Message::future_Message (msg->response, TASK_NAME::ROTATE,
			EVENT_STEPPER_GET_ABS_POS, rotControl->GetAbsolutePosition (), 0 );
			SwitchBoard::send (respMsg );
			break;

		case (EVENT_STEPPER_GET_REL_POS):
			respMsg = Message::future_Message (msg->response, TASK_NAME::ROTATE,
			EVENT_STEPPER_GET_REL_POS, rotControl->GetRelativePosition (), 0 );
			SwitchBoard::send (respMsg );
			break;

		case (EVENT_STEPPER_GET_LOW_LIMIT):
			respMsg = Message::future_Message (msg->response, TASK_NAME::ROTATE,
			EVENT_STEPPER_GET_LOW_LIMIT, rotControl->GetLowerLimit (), 0 );
			SwitchBoard::send (respMsg );
			break;

		case (EVENT_STEPPER_GET_UPR_LIMIT):
			respMsg = Message::future_Message (msg->response, TASK_NAME::ROTATE,
			EVENT_STEPPER_GET_UPR_LIMIT, rotControl->GetUpperLimit (), 0 );
			SwitchBoard::send (respMsg );
			break;

		case (EVENT_STEPPER_GET_REM_TIME):
			respMsg = Message::future_Message (msg->response, TASK_NAME::ROTATE,
			EVENT_STEPPER_GET_REM_TIME, rotControl->GetRemainingTime (), 0 );
			SwitchBoard::send (respMsg );
			break;

		default:
			ESP_LOGE(TAG, "UNKNWON ROTATE EVENT NUMBER %d", msg->event );
			break;
	}
}


/**
 * This is the timer callback - each time thru
 * we call the 'run' function of each StepperMotorController.
 *
 * We *could* define two separate callbacks (with different timers),
 * but being lazy one timer handles both.
 *
 * Note that on occasion we can be called from the message
 * handler, to force an unschedualed loop after a
 * command is issued.
 *
 * In all cases, we expect the timer to be stopped upon entry.
 * We will start it
 */
void StepperDriver::clockCallback(void *arg) {
	static BaseType_t xHigherPriorityTaskWoken=pdFALSE;

	StepperDriver *me= (StepperDriver *)arg;
	xSemaphoreTakeFromISR(me->mylock, &xHigherPriorityTaskWoken);
	me->nodControl->Run();
	me->rotControl->Run();
	int64_t nextNodTime=me->nodControl->GetTimeToNextStep();
	int64_t	nextRotTime=me->rotControl->GetTimeToNextStep();
	int64_t nextTime=(nextNodTime < nextRotTime)? nextNodTime:nextRotTime;
	xSemaphoreGiveFromISR(me->mylock, &xHigherPriorityTaskWoken);
	esp_timer_start_once(me->myTimer, nextTime);
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
	me->nodControl = new StepperMotorController(DIGITAL, NOD_PINA ,NOD_PINB, NOD_PINC, NOD_PIND, 0);
	me->rotControl = new StepperMotorController(NON_DIGITAL,ROTATE_PINA,ROTATE_PINB,ROTATE_PINC,ROTATE_PIND, 0);

	// Initialize the timer
	esp_timer_create_args_t timer_cfg={};
	timer_cfg.callback=&me->clockCallback;
	timer_cfg.name = "stepperTimer";
	timer_cfg.arg = me;
	timer_cfg.dispatch_method=ESP_TIMER_TASK;
	ESP_ERROR_CHECK(esp_timer_create(  &timer_cfg, &(me->myTimer)));

	clockCallback(me);   // First time thru the clock callback.

	// Register us for action
	SwitchBoard::registerDriver(TASK_NAME::NODD, me);
	ESP_LOGI(TAG, "NODD is registered");
	SwitchBoard::registerDriver(TASK_NAME::ROTATE, me);
	ESP_LOGI(TAG, "ROTATE is registered");

	// Sit and twiddle our thumbs while the timer does all the work
	while(true)
	{
		vTaskDelay(10000);
	}

}
