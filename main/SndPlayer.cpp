/**
 * SndPlayer.cpp
 *
 *  Created on: Dec 30, 2021
 *      Author: doug
 *
 * This class plays the mp3 file, and
 * listens for any commands
 */
#include "stdlib.h"
#include "string.h"
#include "SPIFFS.h"
#include "freertos/FreeRTOS.h"
#include "Sequencer/DeviceDef.h"
#include <freertos/task.h>
#include "esp_log.h"
#include <driver/gpio.h>
#include <errno.h>
#include "audio/DACOutput.h"
#include "audio/I2SOutput.h"
#include "Sequencer/Message.h"
#include "Sequencer/SwitchBoard.h"

#include "config.h"
#include "SndPlayer.h"
#include "PwmDriver.h"

#define ENABLE_JAW
#define ENABLE_EYES
#define ENABLE_PWM (defined(ENABLE_JAW) || defined (ENABLE_EYES))

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_STDIO
#include "audio/minimp3.h"

static const char *TAG = "SOUND:";

SndPlayer::SndPlayer (const char *_name) :
		DeviceDef (_name )
{
	runState = PLAYER_IDLE;
	myTask = nullptr;
	eye_scale=1;
	jaw_scale=1;
}

SndPlayer::~SndPlayer ()
{

}

/**
 * This checks for a button push, or for
 * a command thru the 'device' callback
 */
void SndPlayer::checkForCommand ()
{
	uint32_t status;
	static bool lastState = false;
	bool curState;

	if (pdPASS == xTaskNotifyWait (0, 0, &status, 1 ))
	{
		// Turn 'status' into run state.
		runState = (Player_State) status;
		ESP_LOGD(TAG, " See remote command %d", runState);
		return;
	}

	/*  Check button press. Remeber we configured
	 *  negative logic - 0 is button pressed.
	 *
	 *  This routine debounces the button so we
	 *  only respond to (1) A press (ignore releases)
	 *  and (2) Not too close together
	 */
	curState = !gpio_get_level (GPIO_NUM_0 );
//	ESP_LOGD(TAG, "Switch state is %d, last state %d.", curState, lastState );
	if ((curState == true) && (lastState == false))
	{ // Only on transition from off to on...
		ESP_LOGD(TAG, "See button press!");
		switch (runState)
		{
			case (PLAYER_IDLE):
				runState = PLAYER_RUNNING;
				break;

			case (PLAYER_RUNNING):
				runState = PLAYER_PAUSED;
				// TODO:
				break;

			case (PLAYER_PAUSED):
				runState = PLAYER_RUNNING;
				break;

			case (PLAYER_REWIND):
				// TODO: Ignore this - should never happen?
				break;

			default:
				runState = PLAYER_REWIND; // Unknown command!
		}

	}   // end of 'if state changed'
	lastState = curState;
	return;
}

/**
 * Decode - and cause to implement - any external
 * messages.
 */
void SndPlayer::callBack (const Message *msg)
{
	switch (msg->event)
	{
		case (SND_EVENT_PLAYER_START):
			if ((runState == PLAYER_IDLE) || (runState == PLAYER_PAUSED))
			{
				xTaskNotify (myTask, PLAYER_RUNNING, eSetValueWithOverwrite );
			}
			break;

		case (SND_EVENT_PLAYER_PAUSE):
			if (runState == PLAYER_RUNNING)
			{
				xTaskNotify (myTask, PLAYER_PAUSED, eSetValueWithOverwrite );
			}
			break;

		case (SND_EVENT_PLAYER_REWIND): // rewind is also stop!
			if ((runState == PLAYER_RUNNING) || (runState == PLAYER_PAUSED))
			{
				xTaskNotify (myTask, PLAYER_REWIND, eSetValueWithOverwrite );
			}
			break;
	}  // END OF CASE
	return;
}

/**
 * This is where we actually play the music.
 * It is also responsible for sending control
 * info to the eyes and jaw.
 * If the 'prescan' mode is true, then we only read
 * the file without doing any actual output or control, collecting
 * min/max (scaling) info as we go.
 *
 * This is static - a pointer to the current instance
 * is passed in as the argument
 * @param output_ptr - points to the audio output device. Ignored if prescan is true.
 * @param prescan    - if True, then we only scan the file for max/min average, and
 *                     exit immediatly, without doing any eye or jaw movement or audio
 *                     output (output_ptr is ignored, should be nullptr in this case).
 *
 */

void SndPlayer::playMusic (void *output_ptr)
{
	Output *output = (Output*) output_ptr;
	int eye_avg = 0;
	int eye_avg_cnt = 0;
	int jaw_avg = 0;
	int jaw_avg_cnt = 0;
	bool is_output_started = false;


#if defined(ENABLE_JAW) || defined (ENABLE_EYES)
	Message *msg;
#endif

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



	while (1) // WAITING TO START READING THE FILE
	{
		is_output_started = false;
		checkForCommand ();

		if (runState == PLAYER_IDLE)
		{
			vTaskDelay (100 / portTICK_PERIOD_MS );
			continue;
		}

		// mp3 decoder state
		mp3dec_t mp3d = { };
		mp3dec_init (&mp3d );
		mp3dec_frame_info_t info = { };

		// keep track of how much data we have buffered, need to read and decoded
		int to_read = BUFFER_SIZE;
		int buffered = 0;
		int decoded = 0;
		is_output_started = false;

		// this assumes that you have uploaded the mp3 file to the SPIFFS
		errno = 0;
		FILE *fp = fopen (SOURCE_FILE_NAME, "r" );
		if (!fp)
		{
			ESP_LOGE("main", "Failed to open file. Error %d (%s)", errno,
					strerror(errno) );
			runState = PLAYER_IDLE;
			continue;
		}

		while (1) // PLAY THIS FILE
		{
			checkForCommand ();
			if (runState == PLAYER_PAUSED)
			{
				vTaskDelay (100 / portTICK_PERIOD_MS );
				continue;
			}

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

			//ESP_LOGI("main", "Read %d bytes\n", n );
			buffered += n;

			if ((runState == PLAYER_REWIND) || (buffered == 0))
			{
				// Either we've been told to stop, or have reached the end of the file
				// AND processed all the buffered data.
				output->stop ();
				is_output_started = false;
				fclose (fp );
				runState = PLAYER_IDLE;
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
				// if we haven't started the output yet we can do it now as we now know the sample rate and number of channels
				if ( !is_output_started )
				{
					output->start (info.hz );
					is_output_started = true;
				}

				// if we've decoded a frame of mono samples convert it to stereo by duplicating the left channel
				// we can do this in place as our samples buffer has enough space
				// AUDIO
				if (info.channels == 1)
				{
					for (int i = samples - 1; i >= 0; i-- )
					{
						pcm[i * 2] = pcm[i];
						pcm[i * 2 - 1] = pcm[i];
					}
				}

				// This is where we do the averaging
				for (int i = 0; i < (samples * 2); i += 2 )
				{
					eye_avg += abs (pcm[i] );
					eye_avg_cnt++;

					// EYE MOTION
					if (eye_avg_cnt >= EYE_AVG_SIZE)
					{
						eye_avg /= EYE_AVG_SIZE;
#ifdef ENABLE_EYES
						eye_avg = map(eye_avg, 0, 3200, 0, 1000);
						msg = Message::future_Message (TASK_NAME::EYES,
									TASK_NAME::IDLER, EVENT_ACTION_SETVALUE,
									eye_avg * 10, eye_avg * 10, nullptr );
							SwitchBoard::send (msg );
#endif
						eye_avg = 0;
						eye_avg_cnt = 0;
					}


					// JAW MOTION
					jaw_avg += abs (pcm[i] );
					jaw_avg_cnt++;

					if (jaw_avg_cnt >= JAW_AVG_SIZE)
					{
						jaw_avg /= jaw_avg_cnt;
#ifdef ENABLE_JAW
						jaw_avg = map(jaw_avg, 0, 3200, 0, 1000);
						msg = Message::future_Message (TASK_NAME::JAW,
									TASK_NAME::IDLER, EVENT_ACTION_SETVALUE,
									jaw_avg, 0, nullptr );
						SwitchBoard::send (msg );

#endif
						jaw_avg = 0;
						jaw_avg_cnt = 0;
					}

				}

				// write the decoded samples to the I2S output
				output->write (pcm, samples );

				// keep track of how many samples we've decoded
				decoded += samples;
			}
			// ESP_LOGI("main", "decoded %d samples\n", decoded);

		} // END of while PLAY THIS FILE

		msg = Message::future_Message (TASK_NAME::EYES,
											TASK_NAME::IDLER, EVENT_ACTION_SETVALUE,
											0, 0, nullptr );
		SwitchBoard::send (msg );
		msg = Message::future_Message (TASK_NAME::JAW,
											TASK_NAME::IDLER, EVENT_ACTION_SETVALUE,
											0, 0, nullptr );
		SwitchBoard::send(msg);

		ESP_LOGI("main", "Finished playing file\n" );
		fclose (fp );
	}  // END of WAITING TO START READING THE FILE
	ESP_LOGD(TAG, "*******************************OOPS - should not return!***************");

	return;
}

/**
 * This does a short test of the jaw motion and the eyes.
 *
 */
void SndPlayer::testEyesAndJaws ()
{
	Message *msg;

	msg = Message::future_Message (TASK_NAME::EYES, TASK_NAME::IDLER,
	EVENT_ACTION_SETVALUE, 0, 255 , nullptr);
	SwitchBoard::send (msg ); // Left Eye
	msg = Message::future_Message (TASK_NAME::JAW, TASK_NAME::IDLER,
	EVENT_ACTION_SETVALUE, 0, 0, nullptr);
	SwitchBoard::send (msg ); // Open JAW

	vTaskDelay (3000 / portTICK_PERIOD_MS );
	msg = Message::future_Message (TASK_NAME::EYES, TASK_NAME::IDLER,
	EVENT_ACTION_SETVALUE, 255, 0, nullptr );
	SwitchBoard::send (msg );  // Right EYE
	msg = Message::future_Message (TASK_NAME::JAW, TASK_NAME::IDLER,
	EVENT_ACTION_SETVALUE, 1000, 0, nullptr );
	SwitchBoard::send (msg );  // Close JAW

	vTaskDelay (3000 / portTICK_PERIOD_MS );
	ESP_LOGD(TAG, "NOW TO BEGIN..." );
	msg = Message::future_Message (TASK_NAME::EYES, TASK_NAME::IDLER,
	EVENT_ACTION_SETVALUE, 0, 0, nullptr );
	SwitchBoard::send (msg );
	msg = Message::future_Message (TASK_NAME::JAW, TASK_NAME::IDLER,
	EVENT_ACTION_SETVALUE, 0, 0, nullptr );
	SwitchBoard::send (msg );  // Close JAW

}


/**
 * This performs some initialization,
 *    does a short test to check the eyes and jaw,
 *    starts the player task running, and returns.
 * It takes care of the initialization stuff.
 */
void SndPlayer::startPlayerTask (void *_me)
{
	Output *output;
	SndPlayer *me = (SndPlayer*) _me;
	me->runState = PLAYER_IDLE;

	// Register this driver.
	SwitchBoard::registerDriver (TASK_NAME::WAVEFILE, me);

	// This tests eyes and jaws...
	PwmDriver pwm ("eyeball/Servo Driver" );

	// create the output - see config.h for settings
#ifdef USE_I2S
	output = new I2SOutput(I2S_NUM_0), i2s_speaker_pins);
#else
	output = new DACOutput ();
#endif

#ifdef I2S_SPEAKDER_SD_PIN
	// if you I2S amp has a SD pin, you'll need to turn it on
	gpio_set_direction (I2S_SPEAKDER_SD_PIN, GPIO_MODE_OUTPUT );
	gpio_set_level (I2S_SPEAKDER_SD_PIN, 1 );
#endif

	// setup the button to trigger playback - see config.h for settings
	gpio_set_direction (GPIO_BUTTON, GPIO_MODE_INPUT );
	gpio_set_pull_mode (GPIO_BUTTON, GPIO_PULLUP_ONLY );

	// initialize the file system
	SPIFFS spiffs ("/fs" );

#ifdef VOLUME_CONTROL
  // set up the ADC for reading the volume control
  adc1_config_width(ADC_WIDTH_12Bit);
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);
#endif

#if defined(ENABLE_JAW) || defined (ENABLE_EYES)
	me->testEyesAndJaws ();
#endif

	// NOW LETS START THE MUSIC PLAYER...
	//xTaskCreatePinnedToCore(playMusic, "Player", 8092, this, tskIDLE_PRIORITY, &myTask, ASSIGN_MUSIC_CORE);
	me->playMusic (output ); // NOTE: This never returns...
}
