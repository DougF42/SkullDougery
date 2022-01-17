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
#include "Sequencer/DeviceDef.h"
#include "SndPlayer.h"
#include "config.h"
#include "Parameters/RmNvs.h"
#include "Stepper/StepperDriver.h"

static const char *TAG="CmdDecoder::";

CmdDecoder::CmdDecoder (TASK_NAME myId):DeviceDef("Cmd Decoder") {
	respBufSempahore=xSemaphoreCreateBinaryStatic(&respBufSemaphoreBuffer);
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
			ESP_LOGD(TAG, "See NULL string terminator");
			flush();
			break;

		}

		if ((dta[idx] == '\r') || (dta[idx] == '\n'))
		{
//			ESP_LOGD(TAG, "See EOL");
			parseCommand ();
			flush();
			break;

		}

		else if (cmdBufNextChar >= (CMD_BUF_MAX_LEN-1))
		{
			// ERROR - command too long
			postResponse("Error - command too long!", RESPONSE_SYNTAX);
			ESP_LOGD(TAG, "Error - command too long!");
			flush();
			break;

		}

		else
		{

			cmdBuf[cmdBufNextChar++] = dta[idx];
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
	tokens[tokNo]=strtok(cmdBuf, " ");
	if ( tokens[tokNo] == NULL) return;

	// Tokenize the arguments for the command. Stop on too many tokens or end of buffer
	for (tokNo=1; ((tokens[tokNo] != 0) && (tokNo<MAX_ARGUMENT_COUNT)); tokNo++)
	{
		tokens[tokNo] = strtok (NULL, " " );
		if (tokens[tokNo] == NULL) break; // all done
	}

//	ESP_LOGD(TAG, "parseCommand: We have %d tokens\n", tokNo);
//	ESP_LOGD(TAG, "parseCommand: command is %s", tokens[0]);
	dispatchCommand (tokNo, tokens);

	return;
}

/**
 * This will attempt to parse an integer argument.
 * If there is an error, we send a syntax error.
 * return value is the requested integer, or BAD_NUMBER
 */


int CmdDecoder::getIntArg(int tokNo, char *tokens[], int minVal, int maxVal) {
	if (tokens[tokNo]==NULL) {
		ESP_LOGD(TAG, "getIntArg: Missing Argument!");
		postResponse ("Missing Argument!", RESPONSE_SYNTAX );
		return(BAD_NUMBER);
	}

	int val = atoi (tokens[1] );

	if ((val < minVal) || (val > maxVal))
		{
		ESP_LOGD(TAG, "getIntArg - Invalid argument - out of range");
		postResponse ("Invalid argument - out of range", RESPONSE_SYNTAX );
		return(BAD_NUMBER);
		}
	return(val);
}

/**
 * This sends a summary of the available commands.
 *
 */
void CmdDecoder::help() {
	postResponse("Help, show, commit \n",RESPONSE_MORE);
	postResponse(" Player controls:  PAUSE, STOP, RUN", RESPONSE_MORE);
	postResponse(" jaw n    range 0...2000", RESPONSE_MORE);
	postResponse(" eye  n   range 0...8192", RESPONSE_MORE);
	postResponse(" set  key value (see show command output)", RESPONSE_MORE);
	postResponse(" nod(d) or rot(ate) commands:",RESPONSE_MORE);
	postResponse("   ENable DIsable EmergencyStop", RESPONSE_MORE);
	postResponse("   Get: Absolute, Relative, Lower, Upper, Time", RESPONSE_MORE);
	postResponse("   Set: Home, Lower, Upper, Ramp",  RESPONSE_MORE);
	postResponse("   Rotate: Absolute, Relative, Home, Lower, Upper",  RESPONSE_MORE);
	postResponse("  ",RESPONSE_OK);
}


/**
 * This tests the token count, and reports a suitable error
 * message if we dont have the right number of arguments.
 * If arg1 and arg2 are non-null, then we decode those parameters
 * as integers - and return suitable errors if they are not
 * reasonable integers.
 *
 *  tokens[0] orig command.
 *  tokens[1] subcommand.
 *  tokens[2] 1st arg
 *  tokens[3] 2nd arg
 *
 * @param required - the number of tokens required
 * @param tokenCount - the number of tokens in the command
 * @return  true normally, false if wrong number of tokens.
 *          if false, this will post a suitable syntax error
 */
bool CmdDecoder::requireArgs( int tokenCount, char *tokens[], int required, long int *arg1, long int *arg2) {
	char *endptr=NULL;
	if (required > tokenCount)
				{
					postResponse ("Missing argument(s) for this command", RESPONSE_SYNTAX );
					return false;
				}

	if (required < tokenCount)
	{
					postResponse ("Extra text after argument to this command.",	RESPONSE_SYNTAX );
					return false;
	}

	if (arg1 != NULL) {
		*arg1=strtol(tokens[2], &endptr, 10);
		if ( *endptr != '\0') {
			postResponse ("Invalid integer for argument 1", RESPONSE_SYNTAX);
			return false;
		}
	}

	if (arg2 != NULL) {
		*arg2=strtol(tokens[3], &endptr, 10);
		if ( *endptr != '\0') {
			postResponse ("Invalid integer for argument 2", RESPONSE_SYNTAX);
			return false;
		}
	}
	return(true);
}

/**
 * Process the tokenized command, and send an appropriate response.
 *
 * tokens[0] is the command - its guarenteed to be present.
 *
 * tokNo    the number of tokens.
 */
#define ISCMD(_a_) (0==strcasecmp(tokens[0], (_a_)))
#define ISSUBCMD(_a_) (0==strcasecmp(tokens[1], (_a_)))
void CmdDecoder::dispatchCommand (int tokCount, char *tokens[])
{
	Message *msg;
//	ESP_LOGD(TAG, "disspatchCommand - have %d arguments", tokCount);
//	ESP_LOGD(TAG, "disspatchCommand - command is %s", tokens[0]);

	int val;
	if (ISCMD("HELP") || ISCMD("?")) {
		help();

	} else if (ISCMD("SHOW")) {
		showCurSettings();

	} else if (ISCMD("COMMIT")) {
		RmNvs::commit();
		postResponse("OK", RESPONSE_OK);

	} else if (ISCMD("PAUSE")) {
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
		val = getIntArg (1, tokens, 0, 1000 );
		ESP_LOGD(TAG, "Dispatch - jaw=%d", val );
		if (val == BAD_NUMBER)
		{
			postResponse ("ERROR - bad value", RESPONSE_COMMAND_ERRR );
		}
		else
		{
			msg = Message::future_Message (TASK_NAME::JAW, senderTaskName,
			EVENT_ACTION_SETVALUE, val, 0 );
			SwitchBoard::send (msg );
			postResponse ("OK", RESPONSE_OK );
		}
	}
	else if (ISCMD("eye" )) // Set EYE intensity
	{
		ESP_LOGD(TAG, "Dispatch - EYE" );
		val = getIntArg (1, tokens, 0, 1000 );
		if (val == BAD_NUMBER)
		{
			postResponse ("ERROR - bad value", RESPONSE_COMMAND_ERRR );
		}
		else
		{
			ESP_LOGD(TAG, "Dispatch - EYE=%d", val );
			msg = Message::future_Message (TASK_NAME::EYES, senderTaskName,
			EVENT_ACTION_SETVALUE, val, val );
			SwitchBoard::send (msg );
			postResponse ("OK", RESPONSE_OK );
		}
	}
	else if (ISCMD("set")) // any of the SET commands
	{
		setCommands( tokCount, tokens);
	}
	else if (ISCMD("NOD")) // Any of the NOD commands
	{
		stepperCommands(tokCount, tokens);
	}
	else if (ISCMD("ROT")) // Any of the ROTate commands
	{
		stepperCommands(tokCount, tokens);
	}
	else
	{
		// TODO: UNKNOWN COMMAND
		ESP_LOGD(TAG, "Dispatch - unknown command");
		postResponse ("UNKNOWN COMMAND", RESPONSE_UNKNOWN );
	}
	return;
}

/*
 *
 * Ouptut a a lst of the current settings (SHOW)
 */

void CmdDecoder::showCurSettings() {
	const char *bufPtr=nullptr;

	for (int i=0; i<99; i++) {
		bufPtr=RmNvs::get_info(i);
		if (*bufPtr=='\0') break;
		postResponse (bufPtr, RESPONSE_MORE );
	}
	postResponse("END", RESPONSE_OK);
}


/**
 * This will handle any 'set *' command...
 * it is called from dispaychCommand, which has already identified
 *   that the command name is 'set'.
 * This subroutine issues any responses that are needed.
 *
 * @param tokCount - how many tokens were parsed?
 * @param tokens   - character array of pointers to the tokens
 * Available:
 *    SET SSID <txt>
 *    SET PASSWORD <txt>
 *    SET AP <true | false | 1 | 0 | y |n.  Default false.
 */
#define ISARG(_x_, _a_) (0==strcasecmp(tokens[_x_], (_a_)))
void CmdDecoder::setCommands (int tokCount, char *tokens[])
{
	int retVal = 0;

	if (tokCount < 3)
	{
		postResponse ("Missing paramter name or value in SET commnad",
				RESPONSE_UNKNOWN );
		return;
	}

	if (ISARG(1, RMNVS_KEY_WIFI_SSID ))
	{
		retVal = RmNvs::set_str (RMNVS_KEY_WIFI_SSID, tokens[2] );
		postResponse ("OK", RESPONSE_OK );
	}

	else if (ISARG(1, RMNVS_KEY_WIFI_PASS ))
	{
		retVal = RmNvs::set_str (RMNVS_KEY_WIFI_PASS, tokens[2] );
		postResponse ("OK", RESPONSE_OK );
	}

	else if (ISARG(1, RMNVS_FORCE_STA_MODE ))
	{
		char c = tolower (tokens[2][1] );
		bool apstate = (c == '1') || (c = 'y') || (c == 't');
		retVal = RmNvs::set_bool (RMNVS_FORCE_STA_MODE, apstate );
	}

	else if (ISARG(1, RMNVS_IP ))
	{
		retVal = RmNvs::set_addr_as_string (RMNVS_IP, tokens[2] );
		postResponse ("OK", RESPONSE_OK );
	}

	else if (ISARG(1, RMNVS_NETMASK ))
	{
		retVal = RmNvs::set_addr_as_string (RMNVS_NETMASK, tokens[2] );
		postResponse ("OK", RESPONSE_OK );
	}

	else if (ISARG(1, RMNVS_CMD_PORT ))
	{
		uint32_t val = strtol (tokens[2], NULL, 0 );
		if ((val <= 0) || (val >= 65535))
		{
			postResponse ("Port number out of range - must be between 1 and 65535",
					RESPONSE_COMMAND_ERRR );
			return;
		}
		else
		{
			retVal = RmNvs::set_int (RMNVS_CMD_PORT, val );
		}
	}

	else if (ISARG(1, RMNVS_USE_DHCP ))
	{
		char c = tolower (tokens[2][1] );
		bool apstate = (c == '1') || (c == 'y') || (c == 't');
		retVal = RmNvs::set_bool (RMNVS_USE_DHCP, apstate );
	}

	else
	{
		ESP_LOGD(TAG, "setCommands - unknown paramter name" );
		postResponse ("Unknown parameter name in SET command",
				RESPONSE_UNKNOWN );
		return;
	}

	if (retVal == BAD_NUMBER)
	{
		postResponse ("Bad value or parameter in set command",
				RESPONSE_UNKNOWN );
	}
	else
	{
		postResponse ("OK", RESPONSE_OK );
	}

	return;
}


/**
 * This handles any of the 'stepper' commands - either the
 * 'Rot'ational or 'Nodd' commands or requests.
 *
 * @param required - the number of tokens required
 * @param tokenCount - the number of tokens in the command
 *
 */
// TODO: HOW DO WE GET A RESPONSE BACK TO THE CALLER?????
void CmdDecoder::stepperCommands (int tokCount, char *tokens[])
{
	TASK_NAME destination = TASK_NAME::TEST;
	Message *msg = nullptr;
	long int posit = 0;
	long int rate = 0;

	if (ISCMD("ROT" ))
		destination = TASK_NAME::ROTATE;
	else
		destination = TASK_NAME::NODD;

	if (ISSUBCMD("EN" ))   // Enable
	{
		if (!requireArgs (tokCount, tokens, 2, NULL, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_ENABLE, 0, 0 );
		SwitchBoard::send (msg );
		postResponse ("OK", RESPONSE_OK );
	}
	else if (ISSUBCMD("DI" )) // Disable
	{
		if (!requireArgs (tokCount, tokens, 2, NULL, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_DISABLE, 0, 0 );
		SwitchBoard::send (msg );
		postResponse ("OK", RESPONSE_OK );
	}
	else if (ISSUBCMD("SH" )) // Set Home .
	{
		if (!requireArgs (tokCount, tokens, 2, NULL, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_SET_HOME, posit, 0 );
		SwitchBoard::send (msg );
		postResponse ("OK", RESPONSE_OK );
	}
	else if (ISSUBCMD("SL" )) // Set Lower .
	{
		if (!requireArgs (tokCount, tokens, 3, &posit, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_SET_LOWER_LIMIT, posit, 0 );
		SwitchBoard::send (msg );
		postResponse ("OK", RESPONSE_OK );
	}
	else if (ISSUBCMD("SU" )) // Set Upper
	{
		if (!requireArgs (tokCount, tokens, 3, &posit, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_SET_UPPER_LIMIT, posit, 0 );
		SwitchBoard::send (msg );
		postResponse ("OK", RESPONSE_OK );
	}
	else if (ISSUBCMD("SR" )) // Set Ramp
	{
		if (!requireArgs (tokCount, tokens, 2, &posit, NULL )) return;
		if ((posit < 0) || (posit > 9))
		{
			postResponse ("Bad Value", RESPONSE_COMMAND_ERRR );
		}
		else
		{
			msg = Message::future_Message (destination, senderTaskName,
					EVENT_STEPPER_SET_RAMP, posit, 0 );
			SwitchBoard::send (msg );
			postResponse ("OK", RESPONSE_OK );
		}
	}
	else if (ISSUBCMD("RA" ))  // Rotate Abs ...
	{
		if (!requireArgs (tokCount, tokens, 4, &posit, &rate )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_GOABS, posit, rate );
		SwitchBoard::send (msg );
		postResponse ("OK", RESPONSE_OK );
	}
	else if (ISSUBCMD("RR" ))  // Rotate Rel ...
	{
		if (!requireArgs (tokCount, tokens, 4, &posit, &rate )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_GOREL, posit, 0 );
		SwitchBoard::send (msg );
		postResponse ("OK", RESPONSE_OK );
	}
	else if (ISSUBCMD("RH" )) // Rotate Home
	{
		if (!requireArgs (tokCount, tokens, 2, NULL, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_GOHOME, 0, 0 );
		SwitchBoard::send (msg );
		postResponse ("OK", RESPONSE_OK );
	}
	else if (ISSUBCMD("RL" )) // Rotate Lower
	{
		if (!requireArgs (tokCount, tokens, 2, NULL, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_GOLOWER, 0, 0 );
		SwitchBoard::send (msg );
		postResponse ("OK", RESPONSE_OK );
	}
	else if (ISSUBCMD("RU" )) // Rotate Upper
	{
		if (!requireArgs (tokCount, tokens, 2, NULL, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_GOUPPER, 0, 0 );
		SwitchBoard::send (msg );
		postResponse ("OK", RESPONSE_OK );
	}
	else if (ISSUBCMD("ES" )) // E-STOP
	{
		if (!requireArgs (tokCount, tokens, 2, NULL, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_ESTOP, 0, 0 );
		SwitchBoard::send (msg );
		postResponse ("OK", RESPONSE_OK );
	}
	else if (ISSUBCMD("GA" )) // Get Abs position
	{
		if (!requireArgs (tokCount, tokens, 2, NULL, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_GET_ABS_POS, 0, 0 );
		SwitchBoard::send (msg );
		// expect response thru message callback
	}
	else if (ISSUBCMD("GR" )) // Get Rel Position
	{
		if (!requireArgs (tokCount, tokens, 2, NULL, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_GET_REL_POS, 0, 0 );
		SwitchBoard::send (msg );
		// expect response thru message callback
	}
	else if (ISSUBCMD("GL" )) // Get Lower Limit
	{
		if (!requireArgs (tokCount, tokens, 2, NULL, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_GET_LOW_LIMIT, 0, 0 );
		SwitchBoard::send (msg );
		// expect response thru message callback
	}
	else if (ISSUBCMD("GU" )) // get Upper Limit
	{
		if (!requireArgs (tokCount, tokens, 2, NULL, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_GET_UPR_LIMIT, 0, 0 );
		SwitchBoard::send (msg );
		// expect response thru message callback
	}
	else if (ISSUBCMD("GT" )) // Geet remaining time
	{
		if (!requireArgs (tokCount, tokens, 2, NULL, NULL )) return;
		msg = Message::future_Message (destination, senderTaskName,
				EVENT_STEPPER_GET_REM_TIME, 0, 0 );
		SwitchBoard::send (msg );
		// expect response thru message callback
	}
	else
	{
		postResponse ("Unknown command", RESPONSE_SYNTAX );
	}
	return;
}


/**
 * This accepts messages from the various devices, and translates them into
 * a text response, which is then sent out via the 'postResponse' routine which
 * is provided by the channel.
 *
 * To prevent various overrun problems (such as multiple messages from different
 * tasks arriving back-to-back), we use a semaphore to prevent
 * the possibility of a second messsage being delivered before the first is
 * completely dealt with.
 *
 */
void CmdDecoder::callBack(const Message *msg)
{
	xSemaphoreTake(respBufSempahore, (TickType_t ) 10 );
	if ((msg->response == TASK_NAME::NODD)
			|| (msg->response == TASK_NAME::ROTATE))
	{
		switch (msg->event)
		{
			case (EVENT_STEPPER_GET_ABS_POS):
			case (EVENT_STEPPER_GET_REL_POS):
			case (EVENT_STEPPER_GET_LOW_LIMIT):
			case (EVENT_STEPPER_GET_UPR_LIMIT):
				snprintf (respBuf, sizeof(respBuf), " Position is %ld",
						msg->value );
				postResponse (respBuf, RESPONSE_OK );
				break;

			case (EVENT_STEPPER_GET_REM_TIME):
				snprintf (respBuf, sizeof(respBuf), "Time is %ld", msg->value );
				postResponse (respBuf, RESPONSE_OK );
				break;

			default:
				snprintf(respBuf, sizeof(respBuf), "Unkown response from NODD or ROTATE. Event was %d, value %ld",
						msg->event, msg->value);
				postResponse(respBuf, RESPONSE_OK);
				break;
		}

	}
	xSemaphoreGive(respBufSempahore );
}
