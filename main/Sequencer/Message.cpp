/**
 * Message.cpp
 *
 *  Created on: Sep 27, 2021
 *      Author: doug
 *
 */

#include "Message.h"
#include "esp_log.h"

// static const char *TAG="MESSAGE";

Message::Message ()
{
	// TODO Auto-generated constructor stub
	event = EVENT_ACTION_NONE;
	destination=TASK_NAME::IDLER;
	response=TASK_NAME::IDLER;
	actionTime = 0;
	value = 0;
	rate  = 0;

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
	actionTime = oldObj.actionTime;
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
 *
 */
Message *Message::future_Message( TASK_NAME _target,
		TASK_NAME _from, int _event, int _value, int _rate) {
	Message *m=new Message();
	m->event = _event;
	m->destination= _target;
	m->response = TASK_NAME::IDLER;
	m->value = _value;
	m->rate  = _rate;
	return(m);
}

/**
 * DEPRECATED:
 * Factory to create a request for information message, OR
 * a reply to a request for info.
 * 
 *    These are always delivered ASAP.
 * @param target  - what driver should  this be sent to?
 * @param resp    - what driver originated this message?
 * @param event   - What is begin requested, or is this a reply?
 * @param value   - not used if a request. If a reply, this is the value requested
 * @param rate    - not used if a request. If a reply, this is the 'event' from
 *                  the original request, identifying the info being presented.
 *
Message *Message::Info_Message(TASK_NAME _target, TASK_NAME _resp,
		int _event, int _value, int _rate) {
	Message *m=new Message();
	m->event = _event;
	m->response = _resp;
	m->destination= _target;
	m->value = _value;
	m->rate  = _rate;
	return(m);
}
*/
void Message::markNoAction() {
	this->event=EVENT_ACTION_NONE;
}

/**
 * Compare actionTime instances: THIS < rhs
 * (actionTime is used as our priority.)
 */
bool Message::operator < (const Message& rhs) {
	return (this->actionTime < rhs.actionTime);
}

/**
 * Compare actionTime instances: THIS < rhs
 * (actionTime is used as our priority.)
 */
bool Message::operator > (const Message& rhs) {
	return (this->actionTime > rhs.actionTime);
}

