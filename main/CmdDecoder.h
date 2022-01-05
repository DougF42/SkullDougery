/**
 * CmdDecoder.h
 *
 *  Created on: Dec 2, 2021
 *      Author: doug
 *
 * This accepts a command stream, and
 * performs the requested commands.
 *
 * A separate instance is used for each input stream.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "string.h"
#include "Sequencer/Message.h"
#include "config.h"

#ifndef MAIN_CMDDECODER_H_
#define MAIN_CMDDECODER_H_

// Max length of a command line
#define CMD_BUF_MAX_LEN 80
#define MAX_ARGUMENT_COUNT 10
/**
 * 'more' means we need more data. 'reply' means there
 * is a reply to the message. Messages are kept in a
 * queue, so they can be retrieved in the order generated.
 *
 * REPLYS are always a null-terminated text string.
 */
class CmdDecoder
{
public:
	enum responseStatus_t {
		RESPONSE_OK,             // The command was executed correctly
		RESPONSE_COMMAND_ERRR,   // The command was decoded as valid, but execution failed.
		RESPONSE_UNKNOWN,        // The command itself was not recognized
		REPSONSE_EMBEDED_NULL,   // A Null was embeded in the stream.
		RESPONSE_SYNTAX,          // There was a general syntax error.
		RESPONSE_MORE            // There is more to follow this statement...
		};

	CmdDecoder (TASK_NAME myId);
	virtual ~CmdDecoder ();
	void addEOLtoBuffer(void);
	int addToBuffer(const char *dta, int dtaLen);
	void flush();                // Flush messages and input queue.
	virtual void postResponse(const char *respTxt, enum responseStatus_t respcode)=0;

protected:
	void parseCommand();
	void dispatchCommand(int tokCount, char *tokens[]);

private:
	void help();
	char cmdBuf[CMD_BUF_MAX_LEN];
	int  cmdBufNextChar;
	enum TASK_NAME senderTaskName;
	int getIntArg(int tokNo, char *tokens[], int minVal, int maxVal);
	void setCommands (int tokCount, char *tokens[]);
};

#endif /* MAIN_CMDDECODER_H_ */
