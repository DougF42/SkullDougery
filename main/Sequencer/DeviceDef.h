/**
 * DeviceDef.h
 *
 *  Created on: Sep 28, 2021
 *      Author: doug
 *
 * This defines the functions required to define a device.
 * Use this as a virtual base class -
 * *
 * Q: Should 'Sequencer' start the driver?
 * Q: Should we De-Register the driver?
 *
 * CALLBACK:
 *    This is called by the sequencer when an event is supposed to happen.
 *    The msg is only valid for the duration of the call - when the callback returns,
 *    it will be 'freed'.
 */
#include "Message.h"
#ifndef MAIN_SEQUENCER_DEVICEDEF_H_
#define MAIN_SEQUENCER_DEVICEDEF_H_

class DeviceDef
{
public:
	DeviceDef (const char *name) ;
	char *devName; // Pointer to the name of this driver.
	virtual ~DeviceDef () ;
	virtual void callBack(const Message *msg)=0;
};

#endif /* MAIN_SEQUENCER_DEVICEDEF_H_ */
