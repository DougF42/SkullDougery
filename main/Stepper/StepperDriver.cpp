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
#include "string.h"
#include "../config.h"
#include "../Sequencer/SwitchBoard.h"
#include "../Sequencer/DeviceDef.h"


static const char *TAG="STEPPER DRIVER::";

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


#define STOPCLOCK  xSemaphoreTake(mylock, portMAX_DELAY);\
		esp_timer_stop (myTimer );

#define STARTCLOCK xSemaphoreGive(mylock);\
		clockCallback (this );

/**
 * This is how messages (commands) are delivered to us.
 *
 * If the message contains a command to a StepperMotorController,
 * we stop the timer, execute the command, then call the timer interrupt
 * ( which will re-start the timer).
 */
void StepperDriver::callBack (const Message *msg)
{
	STOPCLOCK
	StepperMotorController *target=nullptr;
	Message *respMsg=nullptr;
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


	const char *respText=target->ExecuteCommand(msg->text);
	ESP_LOGD(TAG, "Response:%s", respText);
	respMsg=Message::future_Message(msg->response, msg->destination, msg->event, 0L, 0L, respText);
	SwitchBoard::send(respMsg);
	STARTCLOCK
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
//	uint64_t now= esp_timer_get_time();
	xSemaphoreTake(me->mylock, 3);

	me->nodControl->Run();
	me->rotControl->Run();
//	uint64_t nextNodTime=me->nodControl->GetTimeToNextStep();
//	uint64_t nextRotTime=me->rotControl->GetTimeToNextStep();
//	uint64_t nextTime=(nextNodTime < nextRotTime)? nextNodTime-now:nextRotTime-now;

	xSemaphoreGive(me->mylock);
//	esp_timer_start_once(me->myTimer, nextTime-now);
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
	me->nodControl = new StepperMotorController(DIGITAL, NOD_PINA ,NOD_PINB, NOD_PINC, NOD_PIND, ESP_LED_PIN);
	me->rotControl = new StepperMotorController(NON_DIGITAL,ROTATE_PINA,ROTATE_PINB,ROTATE_PINC,ROTATE_PIND, -1);

	// Initialize the timer
	esp_timer_create_args_t timer_cfg={};
	timer_cfg.callback=&me->clockCallback;
	timer_cfg.arg = param;
	timer_cfg.name = "stepperTimer";
	timer_cfg.dispatch_method=ESP_TIMER_TASK;
	ESP_ERROR_CHECK(esp_timer_create(  &timer_cfg, &(me->myTimer)));

	// Register us for action
	SwitchBoard::registerDriver(TASK_NAME::NODD, me);
	ESP_LOGI(TAG, "NODD is registered");
	SwitchBoard::registerDriver(TASK_NAME::ROTATE, me);
	ESP_LOGI(TAG, "ROTATE is registered");

	//clockCallback(param);   // Force first time thru the clock callback.
	// esp_timer_start_once(me->myTimer, nextTime-now);
	esp_timer_start_periodic(me->myTimer, 5000); // Max step rate 2000 steps per second.



	// Sit and twiddle our thumbs while the timer does all the work
	while(true)
	{
		vTaskDelay(10000);
	}

}
