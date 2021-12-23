/**
 * Messenger.cpp
 *
 *  Created on: Nov 4, 2021
 *      Author: doug
 *
 * This class is used for message passing between various tasks
 * ( usually drivers).
 */

#include <vector>
#include <queue>
#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/semphr.h"

#include "../config.h"
#include "Message.h"
#include "DeviceDef.h"
#include "SwitchBoard.h"

using namespace std;

#define xBlockTime 10000

static const char *TAG="SWITCHBOARD:";
bool volatile SwitchBoard::firstTimeThrough=true;

queue<Message *> SwitchBoard::msgQueue;

TaskHandle_t SwitchBoard::MessengerTaskId;
DeviceDef *SwitchBoard::driverList[NO_OF_TASK_NAMES];


SemaphoreHandle_t SwitchBoard::sequencer_semaphore=nullptr;
StaticSemaphore_t SwitchBoard::sequencer_semaphore_buffer;
#define TAKE_LOCK xSemaphoreTake( sequencer_semaphore, portMAX_DELAY)
#define GIVE_LOCK xSemaphoreGive( sequencer_semaphore)


SwitchBoard::SwitchBoard ()
{
	// Auto-generated constructor stub
}


SwitchBoard::~SwitchBoard ()
{
	// Auto-generated destructor stub
}


/**
 * This is the delivery task.
 *   It waits for a message(s) to be queued, and
 * delivers them to the appropriate task.
 *
 * It should be run as a separate task - it never returns.
 *
 * The argument is not used (it is ignored)
 */
void SwitchBoard::runDelivery(void *xxx) {

	Message *thisMsg;
	ESP_LOGD(TAG, "runDelivery started!");

	if (! firstTimeThrough)
	{
		ESP_LOGE(TAG, "ERROR: runDelivery called more than once!");
		abort();
	}

	//sequencer_semaphore = xSemaphoreCreateBinaryStatic(&sequencer_semaphore_buffer );
	sequencer_semaphore = xSemaphoreCreateBinary( );
	MessengerTaskId = xTaskGetCurrentTaskHandle ();
	GIVE_LOCK;

	firstTimeThrough=false;  // open for buisness

	while(true)
	{
		TAKE_LOCK;
		while (msgQueue.empty())
		{  // Wait for 'send' to queue a message
//			ESP_LOGD(TAG, "Waiting for notify");
			GIVE_LOCK;
			if (0==ulTaskNotifyTake (true, xBlockTime )) {
				TAKE_LOCK;
				continue;
			}
			TAKE_LOCK;
		}

		thisMsg=msgQueue.front();
		msgQueue.pop();
//		ESP_LOGD(TAG, "Past POP. queue size %d",msgQueue.size());
		GIVE_LOCK;
		int devIdx = TASK_IDX(thisMsg->destination );

		if (driverList[devIdx] != nullptr)
			{
//				ESP_LOGD(TAG, "calling device %s", driverList[devIdx]->devName);
				driverList[devIdx]->callBack (thisMsg );
			}
			else
			{
				ESP_LOGE(TAG,
						"SeqLoop - ignored message for undefined device %d",
						TASK_IDX(thisMsg->destination) );
			}
//		ESP_LOGD(TAG, "About to delete msg - normal.");
		delete thisMsg;
	} // end of while(true)
}


/*
 * This adds the message to our queue, and notifies 'runDelivery'
 * to deliver it.
 *
 * The message is no longer available to the caller when this returns,
 *   it will be deleted automatically at an appropriate time.
 *
 *   @param msg - a pointer to the message to queue for delivery.
 */
void SwitchBoard::send(Message *msg) {
	DeviceDef *target;
//	ESP_LOGD(TAG, "::send Trying to take lock!");
	TAKE_LOCK;
//	ESP_LOGD(TAG, "::send Have Lock");
	if (firstTimeThrough) {
		ESP_LOGE(TAG, "::send ERROR: send called before SwitchBoard::runDelivery was run");
		delete msg;
		GIVE_LOCK;
		abort();
	}

	if (nullptr == (target=driverList[static_cast<int>(msg->destination)])) {
	//  		ESP_LOGD(TAG, "::send  Skip message - destination %d not registered",
	 // 		static_cast<int>(msg->destination));
		delete msg;
	} else {
//		ESP_LOGD(TAG, "::send que up message");
		msgQueue.push(msg);
//		ESP_LOGD(TAG, "::send Message queued. About to notify task");
	}
	GIVE_LOCK;
	xTaskNotifyGive (MessengerTaskId);
//	ESP_LOGD(TAG, "::send Task Notified");
}


/**
 * This registers a driver (i.e: A DeviceDef class instance where we can
 *     deliver messages).
 *
 * @param driverName - the TASK_NAME entry for this driver
 * @param me - pointer to the DeviceDef instance to register.
 */
void SwitchBoard::registerDriver(TASK_NAME driverName, DeviceDef *me) {
	TAKE_LOCK;

	if (firstTimeThrough) {
		ESP_LOGE(TAG, "ERROR: send called before SwitchBoard::runDelivery was run");
		GIVE_LOCK;
		abort();
	}


	ESP_LOGD(TAG, "Driver %d is being registered", static_cast<int>(driverName));
	int targIdx=static_cast<int>(driverName);
	if (nullptr != driverList[targIdx])
	{
		ESP_LOGD(TAG, "De-register old driver for %d", TASK_IDX(driverName) );
		driverList[targIdx] = nullptr;
	}
	driverList[targIdx]=me;
	GIVE_LOCK;
	return;
}


/**
 * This removes a driver from the list of known drivers.
 * Any outstanding messages will not be delivered.
 * @param driverName - the name of the type of driver to delete.
 */
void SwitchBoard::deRegisterDriver(TASK_NAME driverName) {
	TAKE_LOCK;
	ESP_LOGD(TAG, "Driver %d is being DE-registered", static_cast<int>(driverName));
	driverList[TASK_IDX(driverName )] = nullptr;
	GIVE_LOCK;
}

/**
 * This empties the queue
 */
void SwitchBoard::flush() {
	TAKE_LOCK;
	while (!msgQueue.empty()) {
		Message *msg=msgQueue.front();
		msgQueue.pop();
		delete msg;
	}
	GIVE_LOCK;
}
