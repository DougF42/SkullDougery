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
#include "Configuration.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "esp_log.h"
#include "SPIFFS.h"
#include "Sequencer/DeviceDef.h"
#include "Sequencer/Message.h"
#include "Sequencer/SwitchBoard.h"

MotionSequencer::MotionSequencer(const char *name) : Device(name) {
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


	FILE *file = fopen(fname);
	if ( !file )
	{
		ESP_LOGE("MotionSequencer", "Failed to open Sequence control file. Error %d (%s)", errno,
				strerror(errno) );
		runState = PLAYER_IDLE;
		continue;
	}
	// Load the file.
	char *linptr=NULL;
	int   *linptrLen=0;
	int actLen;
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
			ESP_LOGE("MotionSequencer", "Format Error detected while reading sequence control file at line %d ", lineno);
			continue;
		}
		if ( firstSeq == nullptr)
		{
			firstSeq = new Sequence(timeStamp, action, value);
			curSeq = firstSeq;
		} else
		{
			curSeq->next = new Sequence(timeStamp, action, value);
			curSeq=curSeq->next;
		}
	}  // End of while
	free(linptr);

	if (! feof(file))
	{
		// AN ERROR OCCURED - report it!
	}


}
