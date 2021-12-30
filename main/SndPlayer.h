/**
 * SndPlayer.h
 *
 *  Created on: Dec 30, 2021
 *      Author: doug
 *
 */

#ifndef MAIN_SNDPLAYER_H_
#define MAIN_SNDPLAYER_H_


#define SND_EVENT_PLAYER_START 100
#define SND_EVENT_PLAYER_PAUSE 101
#define SND_EVENT_PLAYER_REWIND 102

class SndPlayer : DeviceDef
{
public:
	SndPlayer (const char *name);
	virtual ~SndPlayer ();
	void playMusic();
	static void startPlayerTask();
	void callback(const Message *msg);
};

#endif /* MAIN_SNDPLAYER_H_ */
