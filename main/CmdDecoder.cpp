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
#include "SndPlayer.h"

CmdDecoder::CmdDecoder (TASK_NAME myId) {
	senderTaskName=myId;
	flush();
}

CmdDecoder::~CmdDecoder () {
	flush();
}

/**
 * This lets us force an EOL condition.
 */
void CmdDecoder::addEOLtoBuffer(void) {
	char buf='\n';
	addToBuffer(&buf, 1);
	return;
}

/**
 * Add the dta to the command buffer.
 *
 * We only process characters from the input buffer, adding them to the
 * internal command buffer.
 * We return after seeing (1) A NULL character, or (2) An EOL or (3) The end of the buffer.
 * (if EOL is seen, we process the current command  buffer if it is not empty).
 *
 * @param dta - pointer to the dta string.
 * @param dtaLen - number of characters available in the dta string.
 * @return - the number of characters used.
 */
int CmdDecoder::addToBuffer (const char *dta, int dtaLen)
{
	int idx;
	for (idx = 0; idx<dtaLen; idx++)
	{
		if (dta[idx] == '\0')
		{
			break;

		}

		if ((dta[idx] == '\r') || (dta[idx] == '\n'))
		{
			parseCommand ();
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
	} // End of while (idx<dtaLen)
	return (++idx);
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

	// get the command.
	tokens[0]=strtok(cmdBuf, " ");
	if ( tokens[0] == NULL) return;

	// Tokenize the arguments for the command. Stop on too many tokens or end of buffer
	//for (; ((cmdBufPtr != '\0') && (tokNo < MAX_ARGUMENT_COUNT)); cmdBufPtr++)
	for (tokNo=1; ((tokens[tokNo] != 0) && (tokNo<MAX_ARGUMENT_COUNT)); tokNo++)
	{
		tokens[tokNo] = strtok (NULL, " " );
		if (tokens[tokNo] == NULL) break; // all done
	}

	if (0 != strlen (tokens[tokNo] ))
	{
		disspatchCommand (tokNo, tokens);
	}

	return;
}

/**
 * This will attempt to parse an integer argument.
 * If there is an error, we send a syntax error.
 * return value is the requested integer, or BAD_NUMBER
 */
#define BAD_NUMBER 0xFFFFEFE
int CmdDecoder::getIntArg(int tokNo, char *tokens[], int minVal, int maxVal) {
	if (tokens[tokNo]==NULL) {
		postResponse ("Missing Argument!", RESPONSE_SYNTAX );
		return(BAD_NUMBER);
	}

	int val = atoi (tokens[1] );

	if ((val < minVal) || (val > maxVal))
		{
		postResponse ("Invalid argument - out of range", RESPONSE_SYNTAX );
		return(BAD_NUMBER);
		}
	return(val);
}


/**
 * Process the tokenized command, and send an appropriate response.
 *
 * tokens[0] is the command - its guarenteed to be present.
 *
 * tokNo    the number of tokens.
 */
#define ISCMD(_a_) (0==strcasecmp(tokens[0], (_a_)))

void CmdDecoder::disspatchCommand (int tokCount, char *tokens[])
{
	Message *msg;
	int val;
	if (ISCMD("PAUSE")) {
		msg=Message::future_Message(TASK_NAME::WAVEFILE, senderTaskName, SND_EVENT_PLAYER_PAUSE, 0, 0);
		SwitchBoard::send(msg);
		postResponse("OK", RESPONSE_OK);

	} else if (ISCMD("RUN")) {
		msg=Message::future_Message(TASK_NAME::WAVEFILE, senderTaskName, SND_EVENT_PLAYER_START, 0, 0);
		SwitchBoard::send(msg);
		postResponse("OK", RESPONSE_OK);

	} else if (ISCMD("STOP")) {
		msg=Message::future_Message(TASK_NAME::WAVEFILE, senderTaskName, SND_EVENT_PLAYER_REWIND, 0, 0);
		SwitchBoard::send(msg);
		postResponse("OK", RESPONSE_OK);

	} else if (ISCMD("jaw" ))
	{
		val=getIntArg( 1, tokens, 0, 180);

		msg = Message::future_Message (TASK_NAME::JAW, senderTaskName,
				EVENT_ACTION_SETVALUE, val, 0 );
		SwitchBoard::send(msg);
		postResponse ("OK", RESPONSE_OK );
	}
	else if (ISCMD("eye" )) // Set EYE intensity
	{
		val=getIntArg(1, tokens, 0, 10000);
		msg = Message::future_Message (TASK_NAME::EYES, senderTaskName,
				EVENT_ACTION_SETVALUE, val, val );
		SwitchBoard::send(msg);
		postResponse ("OK", RESPONSE_OK );
	}
	else
	{
		// TODO: UNKNOWN COMMAND
		postResponse ("UNKNOWN COMMAND", RESPONSE_UNKNOWN );
	}
	return;
}
