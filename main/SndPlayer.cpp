/**
 * SndPlayer.cpp
 *
 *  Created on: Dec 30, 2021
 *      Author: doug
 *
 * This class plays the mp3 file, and
 * listens for any commands
 */
#include "Sequencer/DeviceDef.h"
#include "SndPlayer.h"

SndPlayer::SndPlayer (const char *_name) : DeviceDef(_name)
{
	// TODO Auto-generated constructor stub

}

SndPlayer::~SndPlayer ()
{
	// TODO Auto-generated destructor stub
}

/**
 * Decode - and cause to implement - any external
 * messages.
 */
void SndPlayer::callback(const Message *msg) {
	switch (msg->event) {
		case (SND_EVENT_PLAYER_START):
				// TODO:
				break;
		case (SND_EVENT_PLAYER_PAUSE):
				// TODO:
				break;
		case(SND_EVENT_PLAYER_REWIND):
				// TODO:
				break;
	}
	return;
}

/**
 * This is where we actually play the music.
 * It is also responsible for sending control
 * info to the eyes and jaw.
 */
void SndPlayer::playMusic() {
	// TODO:
}

/**
 * This is started as a new task.
 * It takes care of the initialization stuff.
 */
void SndPlayer::startPlayerTask() {
	// TODO:
}
