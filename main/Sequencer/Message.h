/**
 * Message.h
 *
 *  Created on: Sep 27, 2021
 *      Author: doug
 * This defines the format of a schedualer request and task event
 *
 * We also define the known tasks here.
 *
 * NOTE: In the interest of speed, and because conflicts just aren't likely
 * for the usage of this data type, we do NOT privatise the variables defining
 * the content of the message.
 */

#ifndef MAIN_SEQUENCER_MESSAGE_H_
#define MAIN_SEQUENCER_MESSAGE_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef uint64_t TIME_t;

// These are the known tasks, and their index
// LAST is not a real task - its a means of determining the highest numbered
//     entry in the TASK_NAME enum.
enum class TASK_NAME
{
	IDLER = 0, WAVEFILE, EYES, JAW, NODD, ROTATE, TEST, UDP, LAST
};
#define NO_OF_TASK_NAMES (static_cast<int> (TASK_NAME::LAST ))
#define TASK_IDX(_xx_)   static_cast<int>(_xx_)


// Event types are int's.
//   values 0...99 are reserved for system event as follows:
//  1 - shut down
//  values above 99 (100...max) are defined by the driver.
//
//   0 means no action.
//     This is a special code that is used two ways:
//       (1) when creating a message intially and
//       (2) The sequencer sets this to indicate that the message
//           has been delivered, and it should be ignored and
//           removed from the queue at the next opertunity.
#define EVENT_ACTION_NONE 0
//  1 means the driver was registered, and is now active.
#define EVENT_ACTION_REGISTERED 1
//   1 means task was unregisterd - shutdown.
#define EVENT_ACTION_SHUTDOWN 2

// This causes an output device to be 'set' to a particular
// value. The meaning of 'set to a particular value' is defined by
// the driver.
#define EVENT_ACTION_SETVALUE 3

class Message
{
public:

	virtual ~Message ();
	Message(const Message &oldObj); // Copy Constructor
	TASK_NAME destination;          // Who to deliver this message to
	TASK_NAME response;             // IF this is a info request, respond to  this destination
	                                // IF this is a response, the originator of the response message.

	// This message to send simple messages, with no response
	static Message *future_Message(TASK_NAME target, TASK_NAME from,
			int _event, long int val, long int rate, const char *txt=nullptr);

	int event;           // A 'valid' event for the device. If the device receives
	                     // an invalid event, ignore it. If this is a response, this
	                     // is the message type of the requesting message.
	long int value;      //  The value we want to set (as defined by the event)
	long int rate;       // An indication of how fast this should happen.
	char text[128];       // Up to 64 bytes null-terminated text.

protected:
	Message ();

};

#endif /* MAIN_SEQUENCER_MESSAGE_H_ */
