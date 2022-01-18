/**
 * Message.cpp
 *
 *  Created on: Sep 27, 2021
 *      Author: doug
 *
 */

#include "Message.h"
#include "esp_log.h"
#include "string.h"

// static const char *TAG="MESSAGE";

Message::Message ()
{
	// TODO Auto-generated constructor stub
	event = EVENT_ACTION_NONE;
	destination=TASK_NAME::IDLER;
	response=TASK_NAME::IDLER;
	value = 0L;
	rate  = 0L;

}


Message::~Message ()
{
//	ESP_LOGD(TAG, "Message delete has been called");
	// TODO Auto-generated destructor stub
}

/**
 * Copy constructor
 */
Message::Message (const Message &oldObj)
{
	destination = oldObj.destination;
	event = oldObj.event;
	response = oldObj.response;
	value = oldObj.value;
	rate = oldObj.rate;
}

/**
 * Factory to create a message to be executed at a particular
 * time in the future.
 * @param abstime - the time when this should trigger
 * @param target  - what driver should handle this?
 * @param from    - what task isthis from?
 * @param event   - What event?
 * @param value   - the first argument for this event
 * @param rate    - the second argument for this event
 * @param txt     - If non-null, then this null-terminated text
 *                  is copied into the message. Max length 64.
 *
 */
Message *Message::future_Message( TASK_NAME _target,
		TASK_NAME _from, int _event, long int _value, long int _rate, const char *txt) {
	Message *m=new Message();
	m->event = _event;
	m->destination= _target;
	m->response = TASK_NAME::IDLER;
	m->value = _value;
	m->rate  = _rate;
	if (txt == nullptr)
		bzero(m->text, sizeof(m->text));
	else
		strncpy(m->text, txt, sizeof(m->text));

	return(m);
}
