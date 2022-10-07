/*
 * MotionDriver.h
 *  This driver reads a sequence control file into memory,
 *  and waits for the music player to send MOTION_SEQ_TIME events
 *  so that we can move the nod and rot position in time with the
 *  music...
 *
 *  Created on: Jul 3, 2022
 *      Author: doug
 */

#ifndef MAIN_SEQUENCER_MOTIONSEQUENCER_H_
#define MAIN_SEQUENCER_MOTIONSEQUENCER_H_

#include "config.h"
#include "Sequencer/DeviceDef.h"
#include "Sequencer/Message.h"
#include "Sequencer/SwitchBoard.h"
#include <errno.h>
#include "esp_log.h"
#include "SPIFFS.h"
// This tells us that the skull has passed a specific time setting (the value in this message),
// and we should take any actions needed to move the skull to the appropriate position.
//
// We jump to the appropriate point in the file, regardless of where the last point was. This
// takes care of any need to watch the run state of a simulation.
#define MOTION_SEQ_TIME 101

class MotionSequencer: public DeviceDef {
private:

	class Sequence {
	public:
		long int tstamp;
		unsigned char event; // N (od)_ or R(ot)
		unsigned char value;  // Value to set the device to.
		Sequence *next;  // Pointer to next
		Sequence *prev;  // Pointer to previous
		Sequence(unsigned long _time, unsigned char _event, unsigned char _value)
		{
			tstamp = _time;
			event  = _event;
			value  = _value;
			next   = nullptr;
			prev   = nullptr;
		}

	};

	Sequence *firstSeq;   // First in the sequence
	Sequence *nextSeqToPerform; // If done in sequence, this should be next
	int seqListSize;

	void findNextAction(unsigned int targetTime, Sequence *ptr);
	int defGetline(char *lineptr, size_t bufLen, FILE *fptr);

public:
	MotionSequencer(const char *name);
	virtual ~MotionSequencer();
	bool loadFile(const char *fname);
	void callBack(const Message *msg); // Message delivery
};

#endif /* MAIN_SEQUENCER_MOTIONSEQUENCER_H_ */
