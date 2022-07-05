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
#include <string.h>
#include <ctype.h>
#include "CmdDecoder.h"
#include "Sequencer/Message.h"
#include "Sequencer/SwitchBoard.h"
#include "Sequencer/DeviceDef.h"
#include "SndPlayer.h"
#include "config.h"
#include "Parameters/RmNvs.h"
#include "Stepper/StepperDriver.h"
#include "PwmDriver.h"

static const char *TAG="CmdDecoder::";

CmdDecoder::CmdDecoder (TASK_NAME myId):DeviceDef("Cmd Decoder") {
	respBufSempahore=xSemaphoreCreateBinaryStatic(&respBufSemaphoreBuffer);
	senderTaskName=myId;
	ESP_LOGD(TAG, "Initialize: sender task name is %d",
			(static_cast<int>(senderTaskName)));
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
	postResponse("Help, show, commit restart \n",RESPONSE_MORE);
	postResponse(" Player controls:  PAUSE, STOP, RUN", RESPONSE_MORE);
	postResponse(" jaw n    range 0...2000", RESPONSE_MORE);
	postResponse(" eye  n   range 0...8192", RESPONSE_MORE);
	postResponse(" set  key value (see show command output)", RESPONSE_MORE);
	postResponse(" nod(d) or rot(ate) - stepper commands:",RESPONSE_MORE);
	postResponse("   ENable DIsable EmergencyStop", RESPONSE_MORE);
	postResponse("   Get: Absolute, Relative, Lower, Upper, Time", RESPONSE_MORE);
	postResponse("   Set: Home, Lower, Upper, Ramp",  RESPONSE_MORE);
	postResponse("   Rotate: Absolute, Relative, Home, Lower, Upper",  RESPONSE_MORE);
	postResponse("  ",RESPONSE_OK);
}


/**
 * This tests the token count, and reports a suitable error
 * message if we dont have the right number of arguments.
 *
 * If arg1 and arg2 are non-null, then we decode those parameters
 * as integers - and return suitable errors if they are not
 * reasonable integers.
 *
 *  tokens[0] orig command. (1 tokens total)
 *  tokens[1] 1st arg       (2 tokens total)
 *  tokens[2] 2nd arg       (3 tokens total)
 *
 * @param tokenCount - Total number of tokens loaded in 'tokens' array.
 * @param tokens[]   - a list of strings, one per token.
 * @param required - the total number of tokens required for this command.
 * @param arg1    - (If not nullptr, this is a pointer to where to store argument 1 decoded as an integer
 * @param arg2    - (If not nullptr, this is a pointer to where to store argument 2 decoded as an integer
 * @return  true normally, false if wrong number of tokens or other error.
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
		*arg1=strtol(tokens[1], &endptr, 10);
		if ( *endptr != '\0') {
			postResponse ("Invalid integer for argument 1", RESPONSE_SYNTAX);
			return false;
		}
	}

	if (arg2 != NULL) {
		*arg2=strtol(tokens[2], &endptr, 10);
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
	ESP_LOGD(TAG, "disspatchCommand - have %d arguments. TOKENS:\n",tokCount);
	for (int idx=0; idx<tokCount; idx++)
	{
		ESP_LOGD(TAG, "  TOKEN %d is `%s`", idx, tokens[idx]);
	}
	ESP_LOGD(TAG, "------");
	long int val;

	if (tokCount==1)
	{
		noArguments(tokCount, tokens);

	}	else if (ISCMD("jaw" ))

	{
		if (!requireArgs (tokCount, tokens, 2, &val, nullptr ))
		{
			postResponse ("ERROR - bad value", RESPONSE_COMMAND_ERRR );
		}
		else
		{
			ESP_LOGD(TAG, "Dispatch - jaw=%ld", val );
			msg = Message::create_message (TASK_NAME::JAW, senderTaskName,
			EVENT_ACTION_SETVALUE, val, 0, nullptr );
			SwitchBoard::send (msg );
			postResponse ("OK", RESPONSE_OK );
		}

	}	else if (ISCMD("eye" )) // Set EYE intensity

	{
		if (!requireArgs (tokCount, tokens, 2, &val, nullptr ))
		{
			postResponse ("ERROR - bad value", RESPONSE_COMMAND_ERRR );
		}
		else
		{
			ESP_LOGD(TAG, "Dispatch - EYE = %ld", val );
			msg = Message::create_message (TASK_NAME::EYES, senderTaskName,
			EVENT_ACTION_SETVALUE, val, val, nullptr );
			SwitchBoard::send (msg );
			postResponse ("OK", RESPONSE_OK );
		}


	}	else if (ISCMD("set" )) // any of the SET commands
	{
		setCommands (tokCount, tokens );

	}	else if (ISCMD("NOD" )) // Any of the NOD commands
	{
		stepperCommands (tokCount, tokens );

	}	else if (ISCMD("ROT" )) // Any of the ROTate commands

	{
		stepperCommands (tokCount, tokens );

	} else	{
		ESP_LOGD(TAG, "Dispatch - unknown command" );
		postResponse ("UNKNOWN COMMAND", RESPONSE_UNKNOWN );
	}
	return;
}

/**
 * No-Argument commands
 * the assumption is that there is only one argument,
 * so either its one of these commands, or its invalid.
 */
void CmdDecoder::noArguments(int tokCount, char *tokens[])
{
	Message *msg;
	if (! requireArgs( tokCount, tokens, 1, nullptr, nullptr)) return;
	if (ISCMD("HELP") || ISCMD("?")) {
		help();

	} else if (ISCMD("SHOW")) { // ignore garbage, if any
		showCurSettings();

	} else if (ISCMD("COMMIT")) {
		RmNvs::commit();
		postResponse("OK", RESPONSE_OK);

	} else if (ISCMD("PAUSE")) {
		msg=Message::create_message(TASK_NAME::WAVEFILE, senderTaskName, SND_EVENT_PLAYER_PAUSE, 0, 0, nullptr);
		SwitchBoard::send(msg);
		postResponse("OK", RESPONSE_OK);

	} else if (ISCMD("RUN")) {
		msg=Message::create_message(TASK_NAME::WAVEFILE, senderTaskName, SND_EVENT_PLAYER_START, 0, 0, nullptr);
		SwitchBoard::send(msg);
		postResponse("OK", RESPONSE_OK);

	} else if (ISCMD("STOP")) {
		msg=Message::create_message(TASK_NAME::WAVEFILE, senderTaskName, SND_EVENT_PLAYER_REWIND, 0, 0, nullptr);
		SwitchBoard::send(msg);
		postResponse("OK", RESPONSE_OK);

	} else if (ISCMD("EYES")) {
		// Report eyeball settings.
		msg=Message::create_message(TASK_NAME::WAVEFILE, senderTaskName, SND_EVENT_PLAYER_REWIND, 0, 0, nullptr);
		SwitchBoard::send(msg);
		postResponse("SORRY-NOT IMPLEMENTED", RESPONSE_COMMAND_ERRR);

	} else	{
		msg=Message::create_message(TASK_NAME::WAVEFILE, senderTaskName, SND_EVENT_PLAYER_REWIND, 0, 0, nullptr);
		SwitchBoard::send(msg);
		postResponse("ERROR-Unknown command", RESPONSE_UNKNOWN);
	}
}


/**
 * @brief decode eyes command.
 * This command can have zero, one OR two arguments.
 *   if zero, then it is decoded by the 'noArguments' routine, and reports
 *         the setting of the two eyes.
 *   if only one, both eyes are set the same.
 *   if two, then the left and right eyes are set independently.
 */
void CmdDecoder::setEyes (int tokCount, char *tokens[])
{
	Message *msg;
	long int left, right = 0;
	if (tokCount == 2)
	{   // set both eyes the same
		if (!requireArgs (tokCount, tokens, 2, &left, nullptr ))
		{
			right = left;
			msg = Message::create_message (TASK_NAME::EYES, senderTaskName,
					EVENT_ACTION_SETLEFT, left, right, nullptr );
			SwitchBoard::send (msg );
			postResponse("OK", RESPONSE_OK);
		} else {
			return; // error - bad or missing value
		}
	}

	if (requireArgs (tokCount, tokens, 3, &left, &right ))
	{   // set left and right eye seperatly
		msg = Message::create_message (TASK_NAME::EYES, senderTaskName,
				EVENT_ACTION_SETLEFT, left, right, nullptr );
		SwitchBoard::send (msg );

		msg = Message::create_message (TASK_NAME::EYES, senderTaskName,
				EVENT_ACTION_SETLEFT, left, right, nullptr );
		SwitchBoard::send (msg );
	} else {
		return;  // error - bad or missing value
	}


	postResponse ("OK", RESPONSE_OK );
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
	Message *msg=NULL;

	if (! requireArgs( tokCount, tokens, 3, NULL, NULL ))
	{
		postResponse ("Missing parameter name or value in SET command",
				RESPONSE_UNKNOWN );
		return;
	}

	if (ISARG(1, "CLEAR")) {
		if (! ISARG(2,"NVS")) {
			postResponse("Bad Command - missing 'NVS' as second argument.", RESPONSE_UNKNOWN);
		}
		else
		{
			msg= Message::create_message(TASK_NAME::NODD, TASK_NAME::IDLER, EVENT_STEPPER_CONTROL_TIMER, false, 0, NULL);
			SwitchBoard::send(msg);
			vTaskDelay(2L);     // Give Switchboard a chance to deliver
			RmNvs::clear_nvs();    // DOES NOT RETURN. DOES automatic RESTART
			postResponse("OK", RESPONSE_OK);
			msg= Message::create_message(TASK_NAME::NODD, TASK_NAME::IDLER, EVENT_STEPPER_CONTROL_TIMER, true, 0, NULL);
						SwitchBoard::send(msg);
		}
	}

	else if (ISARG(1, RMNVS_KEY_WIFI_SSID ))
	{
		retVal = RmNvs::set_str (RMNVS_KEY_WIFI_SSID, tokens[2] );
		postResponse ("OK", RESPONSE_OK );
	}

	else if (ISARG(1, RMNVS_KEY_WIFI_PASS ))
	{
		retVal = RmNvs::set_str (RMNVS_KEY_WIFI_PASS, tokens[2] );
		postResponse ("OK", RESPONSE_OK );
	}

	else if (ISARG(1, RMNVS_WIFI_CHANNEL ))
	{
		uint32_t val = strtol (tokens[2], NULL, 0 );
		if ((val <= 0) || (val >= 32))
		{
			postResponse (
					"Port number out of range - must be between 1 and 32",
					RESPONSE_COMMAND_ERRR );
		}
		else
		{
			retVal = RmNvs::set_int (RMNVS_WIFI_CHANNEL, val );
			if (retVal==BAD_NUMBER) {
				postResponse("Bad number", RESPONSE_SYNTAX);
			}
			else
			{
				postResponse("OK", RESPONSE_OK);
			}
		}
	}

	else if (ISARG(1, RMNVS_FORCE_STA_MODE ))
	{
		char c = tolower (tokens[2][0] );
		bool apstate = (c == '1') || (c = 'y') || (c == 't');
		retVal = RmNvs::set_bool (RMNVS_FORCE_STA_MODE, apstate );
	}

	else if (ISARG(1, RMNVS_IP ))
	{
		retVal = RmNvs::set_addr_as_string (RMNVS_IP, tokens[2] );
		if (retVal==BAD_NUMBER) {
			postResponse("Bad IP address", RESPONSE_SYNTAX);
		}
		else
		{
		postResponse ("OK", RESPONSE_OK );
		}
	}

	else if (ISARG(1, RMNVS_NETMASK ))
	{
		retVal = RmNvs::set_addr_as_string (RMNVS_NETMASK, tokens[2] );
		if (retVal==BAD_NUMBER)
		{
			postResponse("Bad IP address", RESPONSE_SYNTAX);
		}
		else
		{
			postResponse ("OK", RESPONSE_OK );
		}
	}

	else if (ISARG(1, RMNVS_ROUTER_ADDR))
	{
		retVal = RmNvs::set_addr_as_string (RMNVS_ROUTER_ADDR, tokens[2] );
		if (retVal==BAD_NUMBER)
		{
			postResponse("Bad IP address", RESPONSE_SYNTAX);
		}
		else
		{
			postResponse ("OK", RESPONSE_OK );
		}
	}

	else if (ISARG(1, RMNVS_SRV_ADDR))
	{
		retVal = RmNvs::set_addr_as_string (RMNVS_SRV_ADDR, tokens[2] );
		if (retVal==BAD_NUMBER)
		{
			postResponse("Bad IP address", RESPONSE_SYNTAX);
		}
		else
		{
			postResponse ("OK", RESPONSE_OK );
		}
	}

	else if (ISARG(1, RMNVS_SRV_PORT ))
	{
		uint32_t val = strtol (tokens[2], NULL, 0 );
		if ((val <= 0) || (val >= 65535))
		{
			postResponse (
					"Port number out of range - must be between 1 and 65535",
					RESPONSE_COMMAND_ERRR );
		}
		else
		{
			retVal = RmNvs::set_int (RMNVS_SRV_PORT, val );
			if (retVal==BAD_NUMBER) {
				postResponse("Bad number", RESPONSE_SYNTAX);
			}
			else
			{
				postResponse("OK", RESPONSE_OK);
			}
		}
	}

	else if (ISARG(1, RMNVS_DNS_ADDR))
	{
		retVal = RmNvs::set_addr_as_string (RMNVS_DNS_ADDR, tokens[2] );
		if (retVal==BAD_NUMBER)
		{
			postResponse("Bad IP address", RESPONSE_SYNTAX);
		}
		else
		{
			postResponse ("OK", RESPONSE_OK );
		}
	}


	else if (ISARG(1, RMNVS_CMD_PORT ))
	{
		uint32_t val = strtol (tokens[2], NULL, 0 );
		if ((val <= 0) || (val >= 65535))
		{
			postResponse (
					"Port number out of range - must be between 1 and 65535",
					RESPONSE_COMMAND_ERRR );
		}
		else
		{
			retVal = RmNvs::set_int (RMNVS_CMD_PORT, val );
			if (retVal==BAD_NUMBER) {
				postResponse("Bad number", RESPONSE_SYNTAX);
			}
			else
			{
				postResponse("OK", RESPONSE_OK);
			}
		}
	}

	else if (ISARG(1, RMNVS_USE_DHCP ))
	{
		char c = tolower (tokens[2][0] );
		bool apstate = (c == '1') || (c == 'y') || (c == 't');
		retVal = RmNvs::set_bool (RMNVS_USE_DHCP, apstate );
		if (retVal==BAD_NUMBER)
		{
			postResponse("Not a boolean or bad value", RESPONSE_SYNTAX);
		}
		else
		{
			postResponse("OK", RESPONSE_OK);
		}
	}

	else
	{
		ESP_LOGD(TAG, "setCommands - unknown paramter name" );
		postResponse ("Unknown parameter name in SET command",
				RESPONSE_UNKNOWN );
	}

	return;
}


/**
 * This handles any of the 'stepper' commands - either the
 * 'Rot'ational or 'Nodd' commands or requests.
 *
 *
 * Commands: ROT or NOD followed by commands to StepperMotorController.
 *
 *    EStop ENable DIsable SetHome
 *    get: GAbsolute GRelative GLower  GUpper GT??/ GVersion
 *    Motion: RHome RLower Rupper   RAbsolute ttttnnnn RRelative ttttnnnn
 *    set: SLower n   SUpper n  SRamp
 */
void CmdDecoder::stepperCommands (int tokCount, char *tokens[])
{
	TASK_NAME destination = TASK_NAME::TEST;
	Message *msg = nullptr;
	char cmdBuf[128];
	if ( tokCount < 2 )
	{
		postResponse ("ERROR - Missing Argument", RESPONSE_COMMAND_ERRR );
		return;
	}

	// Which driver?
	if (ISCMD("ROT" ))
		destination = TASK_NAME::ROTATE;
	else
		destination = TASK_NAME::NODD;

	// Rebuild the command string so 'stepperMotor->execute' can decode it.
	strcpy(cmdBuf, tokens[1]);
	if (tokCount > 2 )
	{
		strcat(cmdBuf," ");
		strcat(cmdBuf,tokens[2]);
	}

   // Let the 'execute' function handle the specific command.1
	//ESP_LOGD(TAG,"stepperCommand: sender is %d",( static_cast<int> (senderTaskName)));

	msg=Message::create_message(destination, senderTaskName, EVENT_STEPPER_EXECUTE_CMD, 0, 0, cmdBuf);
	SwitchBoard::send(msg);
	return;
}


/**
 * After a device processes a command from the command decoder, it may respond back
 * with a text message indicating its status. (At this time, only the stepper driver
 * does this).
 *
 * This routine in turn takes the callback message, and formats it for the 'postResponse'
 * method of our derived class, so that the response will go back up the channel to
 * whoever (or whatever) originated the command.
 *
 * To prevent various overrun problems (such as multiple messages from different
 * tasks arriving back-to-back), we use a semaphore to prevent
 * the possibility of a second messsage being delivered before the first is
 * completely dealt with.
 *
 */
void CmdDecoder::callBack (const Message *msg)
{
	xSemaphoreTake(respBufSempahore, (TickType_t ) 10 );
	if ((msg->response == TASK_NAME::NODD)
			|| (msg->response == TASK_NAME::ROTATE))
	{

		switch (msg->event)
		{
			case (EVENT_STEPPER_EXECUTE_CMD):
				if (strlen (msg->text ) < 1)
				{
					// Assume a good response
					postResponse ("OK", RESPONSE_OK );
				}
				else
				{
					postResponse (msg->text, RESPONSE_OK );
				}
				break;

			default:
				snprintf (respBuf, sizeof(respBuf),
						"Unknown response to command. Source:%d  Event:%d, value:%ld",
						TASK_IDX(msg->response), msg->event, msg->value);
				postResponse (respBuf, RESPONSE_OK );
				break;
		}

	}
	xSemaphoreGive(respBufSempahore );
}
