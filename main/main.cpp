#include "SPIFFS.h"
#include <stdlib.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <errno.h>
#include <string.h>

#include "audio/DACOutput.h"
#include "audio/I2SOutput.h"
#include "Sequencer/Message.h"
#include "Sequencer/SwitchBoard.h"
#include "config.h"
#include "PwmDriver.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_STDIO

#define ENABLE_PWM_DRIVER

#include "audio/minimp3.h"

const char *TAG = "MAIN::";

extern "C"
{
void app_main ();
}

void wait_for_button_push ()
{
	while (gpio_get_level (GPIO_NUM_0 ) == 1)
	{
		vTaskDelay (pdMS_TO_TICKS(100 ) );
	}
}

const int BUFFER_SIZE = 1024;


/**
 * Reads the MP3 data and decodes it to PCM,
 * and feeds this to the audio output.
 * In addition, it is responsible for averaging the
 * sound level, and directing the eye brightness and
 * the jaw motion.
 *
 * @oaran output - pointer to the output device.
 *
 */

void runProgram(Output *output) {
	int eye_avg=0;
	int eye_avg_cnt=0;
	int jaw_avg=0;
	int jaw_avg_cnt=0;

	// setup for the mp3 decoded
	short *pcm = (short*) malloc (
			sizeof(short) * MINIMP3_MAX_SAMPLES_PER_FRAME );
	uint8_t *input_buf = (uint8_t*) malloc (BUFFER_SIZE );
	if (!pcm)
	{
		ESP_LOGE("main", "Failed to allocate pcm memory" );
	}
	if (!input_buf)
	{
		ESP_LOGE("main", "Failed to allocate input_buf memory" );
	}

	while (true)
	{
		Message *msg;
		// wait for the button to be pushed
		wait_for_button_push ();
		// mp3 decoder state
		mp3dec_t mp3d =
		{ };
		mp3dec_init (&mp3d );
		mp3dec_frame_info_t info =
		{ };
		// keep track of how much data we have buffered, need to read and decoded
		int to_read = BUFFER_SIZE;
		int buffered = 0;
		int decoded = 0;
		bool is_output_started = false;
		// this assumes that you have uploaded the mp3 file to the SPIFFS
		errno = 0;
		FILE *fp = fopen ("/fs/DaysMono.mp3", "r" );
		if (!fp)
		{
			ESP_LOGE("main", "Failed to open file. Error %d (%s)", errno,
					strerror(errno) );
			continue;
		}

		while (1)
		{
#ifdef VOLUME_CONTROL
      auto adc_value = float(adc1_get_raw(VOLUME_CONTROL)) / 4096.0f;
      // make the actual volume match how people hear
      // https://ux.stackexchange.com/questions/79672/why-dont-commercial-products-use-logarithmic-volume-controls
      output->set_volume(adc_value * adc_value);
#endif
			// read in the data that is needed to top up the buffer
			size_t n = fread (input_buf + buffered, 1, to_read, fp );
			// feed the watchdog
			vTaskDelay (pdMS_TO_TICKS(1 ) );
			// ESP_LOGI("main", "Read %d bytes\n", n);
			buffered += n;
			if (buffered == 0)
			{
				// we've reached the end of the file and processed all the buffered data
				output->stop ();
				is_output_started = false;
				break;
			}
			// decode the next frame
			int samples = mp3dec_decode_frame (&mp3d, input_buf, buffered, pcm,
					&info );

			// we've processed this may bytes from teh buffered data
			buffered -= info.frame_bytes;

			// shift the remaining data to the front of the buffer
			memmove (input_buf, input_buf + info.frame_bytes, buffered );

			// we need to top up the buffer from the file
			to_read = info.frame_bytes;
			if (samples > 0)
			{
				ESP_LOGD(TAG, "HAVE %d bytes of data in 1 block", samples);

				// if we haven't started the output yet we can do it now as we now know the sample rate and number of channels
				if (!is_output_started)
				{
					output->start (info.hz );
					is_output_started = true;
				}
				// if we've decoded a frame of mono samples convert it to stereo by duplicating the left channel
				// we can do this in place as our samples buffer has enough space
				// This is also where we do the averaging
				for (int i = samples - 1; i >= 0; i-- )
				{
					eye_avg+=abs(pcm[i]);
					eye_avg_cnt++;

					// EYE MOTION
					if (eye_avg_cnt >=EYE_AVG_SIZE) {
						eye_avg /= EYE_AVG_SIZE;
						// ESP_LOGD(TAG, "SEND IT!   Average = %d", eye_avg);
						msg=Message::future_Message(TASK_NAME::EYES, TASK_NAME::IDLER, EVENT_ACTION_SETVALUE, eye_avg*10, eye_avg*10);
						SwitchBoard::send(msg);
						eye_avg=0;
						eye_avg_cnt=0;
					}

					// JAW MOTION
					jaw_avg += abs(pcm[i]);
					jaw_avg_cnt ++;

					if (jaw_avg_cnt >= JAW_AVG_SIZE) {
			//			ESP_LOGD(TAG, "Jaw average count %d total %d average %d", jaw_avg_cnt, jaw_avg, jaw_avg/jaw_avg_cnt);
						jaw_avg /= jaw_avg_cnt;
						msg=Message::future_Message(TASK_NAME::JAW, TASK_NAME::IDLER, EVENT_ACTION_SETVALUE, jaw_avg, 0);
						SwitchBoard::send(msg);
						jaw_avg=0;
						jaw_avg_cnt=0;
					}

					// AUDIO
					if (info.channels == 1)
					{
						pcm[i * 2] = pcm[i];
						pcm[i * 2 - 1] = pcm[i];
					}

				}
				// write the decoded samples to the I2S output
				output->write (pcm, samples );
				// keep track of how many samples we've decoded
				decoded += samples;
			}
			// ESP_LOGI("main", "decoded %d samples\n", decoded);
		}
		ESP_LOGI("main", "Finished\n" );
		fclose (fp );
	}
}

/**
 * This is the main 'task', which reads the MP3 data and outputs
 * it to audio. This is the setup for the task - the action
 * happens in the 'runProgram' function.
 */
void play_task (void *param)
{

	// create the output - see config.h for settings
#ifdef USE_I2S
  Output *output = new I2SOutput(I2S_NUM_0, i2s_speaker_pins);
#else
	Output *output = new DACOutput ();
#endif

#ifdef I2S_SPEAKDER_SD_PIN
	// if you I2S amp has a SD pin, you'll need to turn it on
	gpio_set_direction (I2S_SPEAKDER_SD_PIN, GPIO_MODE_OUTPUT );
	gpio_set_level (I2S_SPEAKDER_SD_PIN, 1 );
#endif

	// setup the button to trigger playback - see config.h for settings
	gpio_set_direction (GPIO_BUTTON, GPIO_MODE_INPUT );
	gpio_set_pull_mode (GPIO_BUTTON, GPIO_PULLUP_ONLY );

	// create the file system
	SPIFFS spiffs ("/fs" );


	runProgram(output);
}

void app_main ()
{

	TaskHandle_t switchboardHandle;

	// Start Switchboard first
	ESP_LOGD(TAG, "About to start Switchboard!" );
	xTaskCreatePinnedToCore (SwitchBoard::runDelivery, "SwitchBoard", 8192,
			nullptr, 2, &switchboardHandle, ASSIGN_SWITCHBOARD_CORE );

	ESP_LOGD(TAG, "SWITCHBOARD INITIALIZED!\n" );

#ifdef VOLUME_CONTROL
  // set up the ADC for reading the volume control
  adc1_config_width(ADC_WIDTH_12Bit);
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);
#endif

#ifdef ENABLE_PWM_DRIVER
  	Message *msg;
	// This controls eyes and jaws...
	PwmDriver pwm("eyeball/Servo Driver");
	ESP_LOGD(TAG, "PWM is initialized");
	msg=Message::future_Message(TASK_NAME::EYES, TASK_NAME::IDLER, EVENT_ACTION_SETVALUE, 0, 255);
	SwitchBoard::send(msg); // Left Eye
	msg=Message::future_Message(TASK_NAME::JAW, TASK_NAME::IDLER, EVENT_ACTION_SETVALUE, 0, 0);
	SwitchBoard::send(msg); // Open JAW

	vTaskDelay(3000/portTICK_PERIOD_MS);
	msg=Message::future_Message(TASK_NAME::EYES, TASK_NAME::IDLER, EVENT_ACTION_SETVALUE, 255, 0);
	SwitchBoard::send(msg);  // Right EYE
	msg=Message::future_Message(TASK_NAME::JAW, TASK_NAME::IDLER, EVENT_ACTION_SETVALUE, 2000, 0);
	SwitchBoard::send(msg);  // Close JAW

	vTaskDelay(3000/portTICK_PERIOD_MS);
	ESP_LOGD(TAG, "NOW TO BEGIN...");
	msg=Message::future_Message(TASK_NAME::EYES, TASK_NAME::IDLER, EVENT_ACTION_SETVALUE, 0, 0);
	SwitchBoard::send(msg);
	msg=Message::future_Message(TASK_NAME::JAW, TASK_NAME::IDLER, EVENT_ACTION_SETVALUE, 1000, 0);
	SwitchBoard::send(msg);  // Close JAW
#endif

	xTaskCreatePinnedToCore (play_task, "task", 32768, NULL, 1, NULL, 1 );
	while(1) {
		vTaskDelay(5000/portTICK_PERIOD_MS); // Keep me alive
	}
}
