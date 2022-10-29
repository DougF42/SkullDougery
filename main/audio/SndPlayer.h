/**
 * SndPlayer.h
 *
 *  Created on: Dec 30, 2021
 *      Author: doug
 *
 */

#ifndef MAIN_SNDPLAYER_H_
#define MAIN_SNDPLAYER_H_
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// These are commands that can be sent to this device
#define SND_EVENT_PLAYER_IDLE  100
#define SND_EVENT_PLAYER_START 101
#define SND_EVENT_PLAYER_PAUSE 102
#define SND_EVENT_PLAYER_REWIND 103
// also uses EVENT_ACTION_SETVALUE to set volume

const int BUFFER_SIZE = 512;


enum Player_State {
	PLAYER_IDLE,    // Nothin happening. Waiting to start
	PLAYER_RUNNING, // We are playing a file
	PLAYER_PAUSED,  // We paused - file is still open
	PLAYER_REWIND   // We need to stop and close the file.
};

class SndPlayer : DeviceDef
{
public:
	SndPlayer (const char *name);
	virtual ~SndPlayer ();

	void playMusic(void *output_ptr);
	static void startPlayerTask(void *_me);
	void callBack(const Message *msg);
	TaskHandle_t myTask;
	void dump_header(void *info );

private:
	Player_State runState;
	bool is_output_started;
	void checkForCommand();
	void testEyesAndJaws();

	int eye_scale;
	int jaw_scale;
};

#endif /* MAIN_SNDPLAYER_H_ */
