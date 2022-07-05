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

static const char *TAG="MESSAGE";

Message::Message ()
{
	// TODO Auto-generated constructor stub
	event = EVENT_ACTION_NONE;
	destination=TASK_NAME::IDLER;
	response=TASK_NAME::IDLER;
	value = 0L;
	rate  = 0L;
	bzero(text, sizeof(text));
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
	event       = oldObj.event;
	response    = oldObj.response;
	value       = oldObj.value;
	rate        = oldObj.rate;
	memcpy(text, oldObj.text, sizeof(text));
}

/**
 * Factory to create a message to be executed at a particular
 * time in the future.
 * @param _target - who to send message to
 * @param _from   - who task is this from?
 * @param _event  - What event?
 * @param _value  - the first argument for this event
 * @param _rate   - the second argument for this event
 * @param  txt    - If non-null, then this null-terminated text
 *                  is copied into the message. Max length 64.
 *
 */
Message *Message::create_message(
		TASK_NAME _target,	TASK_NAME _from, int _event,
		long int _value, long int _rate, const char *txt) {
	Message *m=new Message();
	m->destination= _target;
	m->response = _from;
	m->event    = _event;
	m->value    = _value;
	m->rate     = _rate;
	bzero(m->text, sizeof(m->text));
	if ((txt != nullptr)&&(*txt!='\0'))
	{
		ESP_LOGD(TAG,"message text length is %u. Message is:%s", strlen(txt), txt);
		strncpy(m->text, txt, sizeof(m->text)-1);
	}
	return(m);
}
