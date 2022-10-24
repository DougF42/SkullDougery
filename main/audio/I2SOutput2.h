/**
 * I2SOutput2.h
 *
 *  Created on: Oct 24, 2022
 *      Author: doug
 *
 */
#ifndef MAIN_AUDIO_I2SOUTPUT2_H_
#define MAIN_AUDIO_I2SOUTPUT2_H_
#include "Output.h"

class I2SOutput2 : Output
{
public:
	I2SOutput2 ();
	virtual ~I2SOutput2 ();
    I2SOutput(i2s_port_t i2s_port, i2s_pin_config_t &i2s_pins);
    void start(uint32_t sample_rate);
};

#endif /* MAIN_AUDIO_I2SOUTPUT2_H_ */
