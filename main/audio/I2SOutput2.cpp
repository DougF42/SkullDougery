/**
 * I2SOutput2.cpp
 *
 *  Created on: Oct 24, 2022
 *      Author: doug
 *
 */

#include "I2SOutput2.h"
#include "../config.h"

// This defines the new channel and initializes it
I2SOutput2::I2SOutput2()
{
	i2s_std_config_t std_cfg = {
	    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
	    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
	    .gpio_cfg = {
	        .mclk = I2S_GPIO_UNUSED,
	        .bclk = I2S_SPEAKER_SERIAL_DATA,
	        .ws = I2S_SPEAKER_LEFT_RIGHT_CLOCK,
	        .dout = GPIO_NUM_18,
	        .din = I2S_GPIO_UNUSED,
	        .invert_flags = {
	            .mclk_inv = false,
	            .bclk_inv = false,
	            .ws_inv = false,
	        },
	    },
	};
	i2s_channel_init_std_mode(tx_handle, &std_cfg);
}

I2SOutput2::~I2SOutput2 ()
{
	// TODO Auto-generated destructor stub
}

