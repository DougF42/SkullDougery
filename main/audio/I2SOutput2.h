/**
 * I2SOutput2.h
 *
 *  Created on: Oct 24, 2022
 *      Author: doug
 *
 */
#ifndef MAIN_AUDIO_I2SOUTPUT2_H_
#define MAIN_AUDIO_I2SOUTPUT2_H_
#include "driver/i2s_std.h"

class I2SOutput2
{
public:
	I2SOutput2 ();
	virtual ~I2SOutput2 ();

	void start (int sample_rate);

	void stop ();

// Override
	void write (int16_t *samples, int frames);
	// Override
	void set_volume (float volume);

};

#endif /* MAIN_AUDIO_I2SOUTPUT2_H_ */
