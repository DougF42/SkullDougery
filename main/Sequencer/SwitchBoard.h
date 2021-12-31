/**
 * Messenger.h
 *
 *  Created on: Nov 4, 2021
 *      Author: doug
 *
 */

#ifndef MAIN_SWITCHBOARD_H_
#define MAIN_SWITCHBOARD_H_

#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "../config.h"
#include "Message.h"
#include "DeviceDef.h"

class SwitchBoard
{
public:

	virtual ~SwitchBoard ();
	static void runDelivery(void *);
	static void send(Message *msg);
	static void registerDriver(TASK_NAME driverName, DeviceDef *me);
	static void deRegisterDriver(TASK_NAME driverName);
	static void flush();

protected:
	SwitchBoard ();

private:
	static volatile bool firstTimeThrough;
	static DeviceDef *driverList[NO_OF_TASK_NAMES];
	static QueueHandle_t msgQueue;
	static TaskHandle_t MessengerTaskId;
	static SemaphoreHandle_t sequencer_semaphore;
	static StaticSemaphore_t sequencer_semaphore_buffer;
};

#endif /* MAIN_SWITCHBOARD_H_ */
