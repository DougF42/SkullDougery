/**
 * CmdDecoder.cpp
 *
 *  Created on: Dec 2, 2021
 *      Author: doug
 *
 * Normal usage:
 *     Each instance of this class handles decoding and processing commands for a single stream.
 *
 *     A stream of characters is fed to the 'parseStream' routine as they become available.
 *
 *     When a command is complete (either we see a cr or lf, OR the 'endline' flag is set),
 *     the command is processed, and a response item is queued (an instance of 'Response' subclass).
 *     The response can be retrieved by calling 'getResponse'.
 *
 *     Note that the 'Response' subclass is completely public, and available to the caller.
 *
 *
 * Commands are formatted as <cmd> <param>
 *    <cmd> and <param> separated by space or tab.
 *    <cmd> is required, <param> is only required for specific commands.
 *

 */
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include "CmdDecoder.h"
#include "Sequencer/Message.h"
#include "Sequencer/SwitchBoard.h"

CmdDecoder::CmdDecoder () {
	flush();
}

CmdDecoder::~CmdDecoder () {
	flush();
}

/**
 * Add the dta to the command buffer.
 * If the EOL is recognized, parse the buffer.
 *
 * @param dta - pointer to the dta string.
 * @param dtaLen - number of characters to process
 * @return - the number of characters used.
 */
int CmdDecoder::addToBuffer (const char *dta, int dtaLen)
{
	int idx = 0;
	while (idx < dtaLen)
	{
		if (dta[idx] == '\0')
		{
			// TODO: RETURN EMBEDED NULL ERROR!
			break;

		}
		else if ((dta[idx] == '\r') || (dta[idx] == '\n'))
		{
			parseCommand ();
			idx++; // because we break here (and dont increment index)
			break;

		}
		else if (cmdBufNextChar >= (CMD_BUF_MAX_LEN-1))
		{
			// TODO: ERROR - command too long
			break;

		}
		else
		{
			cmdBuf[cmdBufNextChar] = dta[idx];
		}
		idx++;
	} // End of while (idx<dtaLen)
	return (idx);
}

// Flush messages and input queue.
void CmdDecoder::flush() {
	cmdBufNextChar=0;
	bzero(cmdBuf, CMD_BUF_MAX_LEN);
}

/**
 * This parses the command string, and builds the argument list.
 * It then calls 'disspatchCommand'.
 */
void CmdDecoder::parseCommand ()
{
	cmdBuf[cmdBufNextChar] = '\0'; // ensure a NULL terminator
	char *tokens[MAX_ARGUMENT_COUNT]; // MAX OF 10 TOKENS!
	int tokNo = 0;
	char *cmdPtr;
	for (cmdPtr = cmdBuf;; ((cmdPtr = NULL) && (tokNo < MAX_ARGUMENT_COUNT)) )
	{
		tokens[tokNo] = strtok (cmdPtr, " " );
		if (tokens[tokNo] == NULL) break;
		tokNo++;
	}

	if (0 != strlen (tokens[tokNo] ))
	{
		disspatchCommand (tokens, tokNo );
	}

	return;
}


/**
 * Process the tokenized command, and send an appropriate response.
 *
 * tokens[0] is the command - its guarenteed to be present.
 *
 * tokNo    the number of tokens.
 */
#define ISCMD(_a_) (0==strcasecmp(tokens[0], (_a_)))

void CmdDecoder::disspatchCommand (char *tokens[], int tokNo)
{
	Message *msg;

	if (ISCMD("jaw" ))
	{
		// TODO:JAW MOTION. arg is 0 to 180
		if (tokNo < 2)
		{
			postResponse ("Missing Argument!", RESPONSE_SYNTAX );
			return;
		}
		int val = atoi (tokens[1] );

		if ((val < 0) || (val > 200))
		{
			postResponse ("Invalid argument!", RESPONSE_SYNTAX );
			return;
		}
		msg = Message::future_Message (TASK_NAME::JAW, TASK_NAME::IDLER,
				EVENT_ACTION_SETVALUE, val, 0 );
		SwitchBoard::send(msg);
		postResponse ("OK", RESPONSE_OK );
		return;
	}
	else if (ISCMD("eye" )) // Set EYE intensity
	{
		// TODO:EYE intensity. arg is 0 to ???
		if (tokNo < 2)
		{
			postResponse ("Missing Argument!", RESPONSE_SYNTAX );
			return;
		}
		int val = atoi (tokens[1] );

		if ((val < 0) || (val > 8000))
		{
			postResponse ("Invalid argument!", RESPONSE_SYNTAX );
			return;
		}
		msg = Message::future_Message (TASK_NAME::EYES, TASK_NAME::IDLER,
				EVENT_ACTION_SETVALUE, val, val );
		SwitchBoard::send(msg);
		postResponse ("OK", RESPONSE_OK );
		return;
	}
	else
	{
		// TODO: UNKNOWN COMMAND
		postResponse ("UNKNOWN COMMAND", RESPONSE_UNKNOWN );
	}

}
