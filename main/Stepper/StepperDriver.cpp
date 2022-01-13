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
#include "../config.h"
#include "../Sequencer/SwitchBoard.h"
#include "../Sequencer/DeviceDef.h"

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
			esp_timer_stop (myTimer );
			nodControl->RotateAbsolute (msg->value, msg->rate );
			clockCallback (this );
			break;

		case (EVENT_STEPPER_SET_HOME):
			esp_timer_stop (myTimer );
			nodControl->SetHomePosition ();
			clockCallback (this );
			break;

		case (EVENT_STEPPER_SET_LOWER_LIMIT):
			esp_timer_stop (myTimer );
			nodControl->SetLowerLimit (msg->value );
			clockCallback (this );
			break;

		case (EVENT_STEPPER_SET_UPPER_LIMIT):
			esp_timer_stop (myTimer );
			nodControl->SetUpperLimit (msg->value );
			clockCallback (this );
			break;

		case (EVENT_STEPPER_SET_RAMP):
			esp_timer_stop (myTimer );
			nodControl->SetRamp (msg->value );
			clockCallback (this );
			break;

		case (EVENT_STEPPER_GOHOME):
			esp_timer_stop (myTimer );
			nodControl->RotateToHome ();
			clockCallback (this );
			break;

		case (EVENT_STEPPER_GOUPPER):
			esp_timer_stop (myTimer );
			nodControl->RotateToUpperLimit ();
			clockCallback (this );
			break;

		case (EVENT_STEPPER_GOLOWER):
			esp_timer_stop (myTimer );
			nodControl->RotateToLowerLimit ();
			clockCallback (this );
			break;

		case (EVENT_STEPPER_ESTOP):
			esp_timer_stop (myTimer );
			nodControl->EStop ();
			clockCallback (this );
			break;

		case (EVENT_STEPPER_ENABLE):
			esp_timer_stop (myTimer );
			nodControl->Enable ();
			clockCallback (this );
			break;

		case (EVENT_STEPPER_DISABLE):
			esp_timer_stop (myTimer );
			nodControl->Disable ();
			clockCallback (this );
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
			esp_timer_stop (myTimer );
			rotControl->RotateAbsolute (msg->value, msg->rate );
			clockCallback (this );
			break;

		case (EVENT_STEPPER_SET_HOME):
			esp_timer_stop (myTimer );
			rotControl->SetHomePosition ();
			clockCallback (this );
			break;

		case (EVENT_STEPPER_SET_LOWER_LIMIT):
			esp_timer_stop (myTimer );
			rotControl->SetLowerLimit (msg->value );
			clockCallback (this );
			break;

		case (EVENT_STEPPER_SET_UPPER_LIMIT):
			esp_timer_stop (myTimer );
			rotControl->SetUpperLimit (msg->value );
			clockCallback (this );
			break;

		case (EVENT_STEPPER_SET_RAMP):
			esp_timer_stop (myTimer );
			rotControl->SetRamp (msg->value );
			clockCallback (this );
			break;

		case (EVENT_STEPPER_GOHOME):
			esp_timer_stop (myTimer );
			rotControl->RotateToHome ();
			clockCallback (this );
			break;

		case (EVENT_STEPPER_GOUPPER):
			esp_timer_stop (myTimer );
			rotControl->RotateToUpperLimit ();
			clockCallback (this );
			break;

		case (EVENT_STEPPER_GOLOWER):
			esp_timer_stop (myTimer );
			rotControl->RotateToLowerLimit ();
			clockCallback (this );
			break;

		case (EVENT_STEPPER_ESTOP):
			esp_timer_stop (myTimer );
			rotControl->EStop ();
			clockCallback (this );
			break;

		case (EVENT_STEPPER_ENABLE):
			esp_timer_stop (myTimer );
			rotControl->Enable ();
			clockCallback (this );
			break;

		case (EVENT_STEPPER_DISABLE):
			esp_timer_stop (myTimer );
			rotControl->Disable ();
			clockCallback (this );
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
			ESP_LOGE(TAG, "UNKNWON ROTATE EVENT NUMBER %d",msg->event );
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
	StepperDriver *me= (StepperDriver *)arg;
	me->nodControl->Run();
	me->rotControl->Run();
	int64_t nextNodTime=me->nodControl->GetTimeToNextStep();
	int64_t	nextRotTime=me->rotControl->GetTimeToNextStep();
	int64_t nextTime=(nextNodTime < nextRotTime)? nextNodTime:nextRotTime;
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
	ESP_ERROR_CHECK(esp_timer_create(  &timer_cfg, &(me->myTimer)));

	// Register us for action
	SwitchBoard::registerDriver(TASK_NAME::NODD, me);
	SwitchBoard::registerDriver(TASK_NAME::ROTATE, me);

	// Sit and twiddle our thumbs while the timer does all the work
	while(true)
	{
		vTaskDelay(10000);
	}


}
