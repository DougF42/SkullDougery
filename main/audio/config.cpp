#include "config.h"
#include <freertos/FreeRTOS.h>
#include <driver/i2s.h>

// i2s speaker pins
i2s_pin_config_t i2s_speaker_pins = {
	.mck_io_num = 3,     // DEF: TBD
    .bck_io_num = I2S_SPEAKER_SERIAL_CLOCK,
    .ws_io_num = I2S_SPEAKER_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_SPEAKER_SERIAL_DATA,
    .data_in_num = I2S_PIN_NO_CHANGE,
};
