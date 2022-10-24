/**
 * I2SOutput2.cpp
 *
 *  Created on: Oct 24, 2022
 *      Author: doug
 *
 */
#include <freertos/FreeRTOS.h>
#include <driver/i2s_std.h>
#include "I2SOutput2.h"
#include "../config.h"
static const char *TAG="***I2SOUPUT:"
// This defines the new channel and initializes it
I2SOutput2::I2SOutput2()
{
	i2s_std_config_t std_cfg = {
	    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
	    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
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
	// TBD: i2s_channel_init_std_mode(tx_handle, &std_cfg);
}



void I2SOutput2::start(int sample_rate)
{

}

void I2SOutput2::stop()
{

}


// TBD: Do we want this version of 'write?'
#define NUM_FRAMES_TO_SEND 128
void I2SOutput2::write(int16_t *samples, int frames)
{
	// TODO: Device output? void Output::write(int16_t *samples, int frames)
	{
	  // this will contain the prepared samples for sending to the I2S device
	  int frame_index = 0;
	  while (frame_index < frames)
	  {
	    // fill up the frames buffer with the next NUM_FRAMES_TO_SEND frames
	    int frames_to_send = 0;
	    for (int i = 0; i < NUM_FRAMES_TO_SEND && frame_index < frames; i++)
	    {
	      int left_sample = process_sample(volume * float(samples[frame_index * 2]));
	      int right_sample = process_sample(volume * float(samples[frame_index * 2 + 1]));
	      frames_buffer[i * 2] = left_sample;
	      frames_buffer[i * 2 + 1] = right_sample;
	      frames_to_send++;
	      frame_index++;
	    }
	    // write data to the i2s peripheral - this will block until the data is sent
	    size_t bytes_written = 0;
	    //i2s_write(m_i2s_port, frames_buffer, frames_to_send * sizeof(int16_t) * 2, &bytes_written, portMAX_DELAY);
	    if (bytes_written != frames_to_send * sizeof(int16_t) * 2)
	    {
	      ESP_LOGE(TAG, "Did not write all bytes");
	    }
	  }
	}

}


//
void I2SOutput2::set_volume(float volume)
{
	return; // NOP in this implementation
}
