/**
 * Messenger.cpp
 *
 *  Created on: Nov 4, 2021
 *      Author: doug
 *
 * This class is used for message passing between various tasks
 * ( usually drivers).
 *
 * The locking protects the driverList from changes while we
 * are trying to deliver messages.
 */

#include <vector>

#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
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

// Store pointers to messages (we DONT copy the message itself!)
StaticQueue_t msgQueueBuffer;
uint8_t      msgQueueStorage[sizeof(Message *)*11];
//QueueHandle_t SwitchBoard::msgQueue=xQueueCreateStatic( 10, sizeof(Message *), msgQueueStorage, &msgQueueBuffer);
QueueHandle_t SwitchBoard::msgQueue=xQueueCreate( 10, sizeof(Message *));

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

	Message *thisMsg=nullptr;
	ESP_LOGD(TAG, "runDelivery started!");
	//GIVE_LOCK;
	if (! firstTimeThrough)
	{
		ESP_LOGE(TAG, "ERROR: runDelivery called more than once!");
		abort();
	}

	sequencer_semaphore = xSemaphoreCreateBinary( );
	MessengerTaskId = xTaskGetCurrentTaskHandle ();
	//GIVE_LOCK;
	firstTimeThrough=false;  // Now open for buisness

	while(true)
	{
		while ( 0 == uxQueueMessagesWaiting(msgQueue))
		{  // Wait for 'send' to queue a message
//			ESP_LOGD(TAG, "Waiting for notify");
			if (0==ulTaskNotifyTake (true, xBlockTime )) {
				continue;
			}
		}

//		ESP_LOGD(TAG, "About to pop message");
		xQueueReceive( msgQueue, &thisMsg, 1);
//		ESP_LOGD(TAG, "Past POP. thisMsg is at %p", thisMsg);
		//TAKE_LOCK;
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
//		ESP_LOGD(TAG, "Msg delivered or skipped. About to delete msg.");
		//GIVE_LOCK;
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
//	ESP_LOGD(TAG, "IN SEND");
	if (firstTimeThrough) {
		ESP_LOGE(TAG, "::send ERROR: send called before SwitchBoard::runDelivery was run");
		delete msg;
		abort();
	}

	//TAKE_LOCK;
//	ESP_LOGD(TAG, "::Sending message. Addr is %p", msg);
	xQueueSend(msgQueue, (void *) &msg, 1);
//	ESP_LOGD(TAG, "::send   Message sent");
	xTaskNotifyGive (MessengerTaskId);

}


/**
 * This registers a driver (i.e: A DeviceDef class instance where we can
 *     deliver messages).
 *
 * @param driverName - the TASK_NAME entry for this driver
 * @param me - pointer to the DeviceDef instance to register.
 */
void SwitchBoard::registerDriver(TASK_NAME driverName, DeviceDef *me) {
	ESP_LOGD(TAG, "In register driver...");
	//TAKE_LOCK;
	ESP_LOGD(TAG, "Have lock in Register driver...");
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
	//GIVE_LOCK;
	return;
}


/**
 * This removes a driver from the list of known drivers.
 * Any outstanding messages will not be delivered.
 * @param driverName - the name of the type of driver to delete.
 */
void SwitchBoard::deRegisterDriver(TASK_NAME driverName) {
	// TAKE_LOCK;
	ESP_LOGD(TAG, "Driver %d is being DE-registered", static_cast<int>(driverName));
	driverList[TASK_IDX(driverName )] = nullptr;
	// GIVE_LOCK;
}

/**
 * This empties the queue
 */
void SwitchBoard::flush() {
	Message *msg=nullptr;
	// TAKE_LOCK;
	while (0 != uxQueueMessagesWaiting(msgQueue)) {
		xQueueReceive(msgQueue, msg, 0);
		delete msg;
	}
	// GIVE_LOCK;
}
