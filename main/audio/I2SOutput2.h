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
	I2SOutput2 (uint32_t rate, i2s_data_bit_width_t bits_per_sample);
	virtual ~I2SOutput2 ();

	void start (int sample_rate);
	void stop ();
	void write (const void *samples, int bytes_in_buffer);
	void set_volume (float volume);

};

#endif /* MAIN_AUDIO_I2SOUTPUT2_H_ */
