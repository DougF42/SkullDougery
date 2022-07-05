/*
 * MotionDriver.cpp
 *
 * The sequence file is formatted one line per action as follows:
 * (all fields are textual, separated by space. terminate with EOL.
 * '# at head of line is comment.
 * FORMAT:
 *  X <int sample_number>  <int action>, <int value> <eol>
 *  Created on: Jul 3, 2022
 *      Author: doug
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "esp_log.h"
#include "SPIFFS.h"
#include "Sequencer/DeviceDef.h"
#include "Sequencer/Message.h"
#include "Sequencer/SwitchBoard.h"
#include "MotionSequencer.h"

static const char *TAG = "MotionSequencer";

MotionSequencer::MotionSequencer(const char *name) : DeviceDef(name) {
	// TODO Auto-generated constructor stub

	ESP_LOGD(TAG, "Register Motion Sequencer");
	SwitchBoard::registerDriver (TASK_NAME::MOTIONSEQ, this );


}

MotionSequencer::~MotionSequencer() {
	// TODO Auto-generated destructor stub
}

/**
 * Load the indicated motion file from flash
 * '# at head of line is comment.
 * FORMAT:
 *  <int sample_number>  <int action>, <int value> <eol>
 */
bool MotionSequencer::loadFile(const char *fname)
{
	Sequence *curSeq;

	// If there was a previous load, delete it.
	while (firstSeq != nullptr)
	{
		curSeq = firstSeq->next;
		delete firstSeq;
		firstSeq = curSeq;
	}


	FILE *file = fopen(fname, "r");
	if ( !file )
	{
		ESP_LOGE("MotionSequencer", "Failed to open Sequence control file. Error %d (%s)", errno,
				strerror(errno) );
		return (false);
	}

	// Load the file.
	char *linptr=NULL;
	size_t linptrLen=0;
	ssize_t actlen;
	long int lineno=0;

	unsigned long timeStamp;
	unsigned char action;
	unsigned char value;
	curSeq=nullptr;

	while ( 0 < (actlen=getline( &linptr, &linptrLen, file)))
	{
		// process the line:
		lineno++;
		if (linptr[0] == '#') continue; // Skip comments
		int noOfArgs = sscanf(linptr, "%lu %c %c", &timeStamp, &action, &value );
		if (noOfArgs != 3)
		{
			ESP_LOGE(TAG, "Format Error detected while reading sequence control file at line %ld ", lineno);
			continue;
		}

		if (timeStamp < curSeq->tstamp)
		{
			ESP_LOGE(TAG, "Sequence error: Time of line %ld is less than previous!", lineno);
			continue;
		}


		if ( firstSeq == nullptr)
		{
			firstSeq = new Sequence(timeStamp, action, value);
			curSeq = firstSeq;
		} else
		{
			curSeq->next = new Sequence(timeStamp, action, value);
			(curSeq->next)->prev = curSeq;
			curSeq=curSeq->next;
		}

	}  // End of while
	free(linptr);

	if (! feof(file))
	{
		// AN ERROR OCCURED - report it!
	}

	fclose(file);
	// All set....
	nextSeqToPerform = firstSeq;

	return(true);
}

/**
 * Find the next action to be performed.
 *
 * Locate the first action that can be performed at this
 * time.
 *
 * @param targetTime - the current 'time' (i.e.: sample number)
 * @param ptr  - pointer to 'current' Sequencer object.
 */
 void MotionSequencer::findNextAction(unsigned int targetTime, Sequence *ptr)
 {
	 Sequence *seq = ptr;
	 // move backwards until prev
	 while (seq != nullptr)
	 {
		 if ( seq->tstamp < targetTime) break;
		 seq=seq->prev;
	 } // Try next one
	 ptr=seq;
	 return;
}

/**
 * This is where we receive messages about when to take action
 */
void MotionSequencer::callBack(const Message *msg)
{
	// We only respond to one event type...
	if (msg->event != MOTION_SEQ_TIME) return;


}
