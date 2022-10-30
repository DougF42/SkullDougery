/**
 * @file: SndPlayer.cpp
 *
 *  Created on: Dec 30, 2021
 *      Author: doug
 *
 * This class plays the mp3 file, and
 * listens for any commands
 */
#include "stdlib.h"
#include "string.h"
#include "../SPIFFS.h"
#include "freertos/FreeRTOS.h"
#include "../Sequencer/DeviceDef.h"
#include <freertos/task.h>
#include "esp_log.h"
#include <driver/gpio.h>
#include <errno.h>

#include "I2SOutput2.h"
#include "../Sequencer/Message.h"
#include "../Sequencer/SwitchBoard.h"

#include "config.h"
#include "SndPlayer.h"
#include "../PwmDriver.h"



// 8000 samples is aprox 1 second.
#define NOTIFYINTERVAL 8000

// #define ENABLE_JAW
// #define ENABLE_EYES
#define ENABLE_PWM (defined(ENABLE_JAW) || defined (ENABLE_EYES))

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_STDIO
#include "minimp3.h"

static const char *TAG = "SNDPLAYER:";

SndPlayer::SndPlayer (const char *_name) :
		DeviceDef (_name )
{
	runState = PLAYER_IDLE;
	is_output_started = false;
	myTask = nullptr;
	eye_scale=1;
	jaw_scale=1;
}


// Destructor
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
				break;

			case (PLAYER_PAUSED):
				runState = PLAYER_RUNNING;
				break;

			case (PLAYER_REWIND):
				// Ignore this - should never happen?
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
 * This routine averges the current pcm data to determine
 * the eye and jaw position. Later we may also add
 * automatic volume controls...
 * @param: pcm pointer to the buffer with PCM data.
 *         We expect 16 bytes per samle, stereo.
 * @param: samples - the number of samples.
 *         One pair(left/right channel) is considered 1 sample.
 */
void SndPlayer::moveEyesAndJaw(PCM_t *pcm, int samples)
{
#ifdef ENABLE_EYE
  int eye_avg = 0;
  int eye_avg_cnt = 0;
#endif

#ifdef ENABLE_JAW
  int jaw_avg = 0;
  int jaw_avg_cnt = 0;
#endif

#if defined(ENABLE_EYE) || defined(ENABLE_JAW)
  Message *msg;

  // This is where we do the averaging
  for (int i = 0; i < (samples * 2); i += 2 )
    {
#ifdef ENABLE_EYES
      eye_avg += abs (pcm[i] );
      eye_avg_cnt++;

      // EYE MOTION
      if (eye_avg_cnt >= EYE_AVG_SIZE)
	{
	  eye_avg /= EYE_AVG_SIZE;
	  
	  eye_avg = map(eye_avg, 0, 3200, 0, 1000);
	  msg = Message::create_message (TASK_NAME::EYES,
					 TASK_NAME::IDLER, EVENT_ACTION_SETVALUE,
					 eye_avg * 10, eye_avg * 10, nullptr );
	  SwitchBoard::send (msg );  
	  eye_avg = 0;
	  eye_avg_cnt = 0;
	}
#endif

#ifdef ENABLE_JAW
      // JAW MOTION
      jaw_avg += abs (pcm[i] );
      jaw_avg_cnt++;
      
      if (jaw_avg_cnt >= JAW_AVG_SIZE)
	{
	  jaw_avg /= jaw_avg_cnt;
	  
	  jaw_avg = map(jaw_avg, 0, 3200, 0, 1000);
	  msg = Message::create_message (TASK_NAME::JAW,
					 TASK_NAME::IDLER, EVENT_ACTION_SETVALUE,
					 jaw_avg, 0, nullptr );
	  SwitchBoard::send (msg );
	  
	  
	  jaw_avg = 0;
	  jaw_avg_cnt = 0;
	}
#endif

#ifdef VOLUME_CONTROL
      auto adc_value = float(adc1_get_raw(VOLUME_CONTROL)) / 4096.0f;
      // make the actual volume match how people hear
      // https://ux.stackexchange.com/questions/79672/why-dont-commercial-products-use-logarithmic-volume-controls
      output->set_volume(adc_value * adc_value);
#endif
    }
#endif
}
  
/**
 * This is where we actually play the music.
 * It is also responsible for sending control
 * info to the eyes and jaw.
 * If the 'prescan' mode is true, then we only read
 * the file without doing any actual output or control, collecting
 * min/max (scaling) info as we go.
 *
 * @param output_ptr - points to the audio output device. Ignored if prescan is true.
 *
 */
void SndPlayer::playMusic (void *output_ptr)
{
	FILE *fp;
	I2SOutput2 *output = (I2SOutput2*) output_ptr;
	is_output_started = false;

	// setup the data buffer for the decoded PCM data
	PCM_t *pcm = (PCM_t*) malloc (sizeof(PCM_t) * MINIMP3_MAX_SAMPLES_PER_FRAME );
	if (!pcm)
	{
		ESP_LOGE(TAG, "Failed to allocate pcm memory" );
	}
	
	uint8_t *input_buf = (uint8_t*) malloc (BUFFER_SIZE );
	if (!input_buf)
	{
		ESP_LOGE(TAG, "Failed to allocate input_buf memory" );
	}

	while (1) // *** WAITING TO START READING THE FILE
	{
		is_output_started = false;
		checkForCommand ();

		if (runState == PLAYER_IDLE)
		{
			vTaskDelay (100 / portTICK_PERIOD_MS );
			continue;
		}

		// init the mp3 decoder logic
		mp3dec_t mp3d = { };
		mp3dec_init (&mp3d );
		mp3dec_frame_info_t info = { };

		// keep track of how much data we have buffered, need to read.
		int to_read = BUFFER_SIZE;
		int buffered = 0;


		// this assumes that you have uploaded the mp3 file to the SPIFFS
		errno = 0;
		fp = fopen (SOURCE_FILE_NAME, "r" );
		if (!fp)
		{
			ESP_LOGE("main", "Failed to open file. Error %d (%s)", errno,
					strerror(errno) );
			runState = PLAYER_IDLE;
			continue;
		}

		while (1) // *** PLAY THIS FILE
		{
			checkForCommand ();
			if (runState == PLAYER_PAUSED)
			{
				vTaskDelay (100 / portTICK_PERIOD_MS );
				continue;
			}

			vTaskDelay (1); // Feed the watchdog
			if (to_read >= (BUFFER_SIZE-300))
			{
				// read in the data that is needed to top up the buffer
				size_t n = fread (input_buf + buffered, 1, to_read, fp );
				buffered += n;
				to_read-=n;
			}

			if ((runState == PLAYER_REWIND) || (buffered == 0))
			{   // Either we've been told to stop, or have reached the end of the file
				// AND processed all the buffered data.
				ESP_LOGD(TAG, "stopping- see runState %d buffered %d", runState, buffered);
				output->stop ();
				is_output_started = false;
				fclose (fp );
				runState = PLAYER_IDLE;
				break;
			}


			int samples = mp3dec_decode_frame (&mp3d, input_buf, buffered, pcm, &info );
			buffered -= info.frame_bytes;
			memmove (input_buf, input_buf + info.frame_bytes, buffered ); // shift to front
			to_read += info.frame_bytes; // do this on the next pass...
			if (samples > 0)
			{ // we have data!
				/* if we haven't started the output yet we can do it now as we
				 * now know the sample rate and number of channels
				 */
				if ( !is_output_started )
				{
					output->start (info.hz );
					is_output_started = true;
					dump_header (&info);
				}

				// if we've decoded a frame of mono samples convert it to stereo by duplicating the left channel
				// we can do this in place as our samples buffer has enough space.
				// assumption: 16-bit (2 byte) sample size.
				if (info.channels == 1)
				{
					for (int i = samples - 1; i >= 0; i-- )
					{
						pcm[i * 2] = pcm[i];
						pcm[i * 2 - 1] = pcm[i];
					}
				}

				// write the decoded samples to the output.
				output->write (pcm, samples * 2); // stereo !
				moveEyesAndJaw( pcm, samples); // process eyes/jaw/volume
			}
			// ESP_LOGI("main", "decoded %d samples\n", decoded);

		} // END of while PLAY THIS FILE

#ifdef ENABLE_EYES
		msg = Message::create_message (TASK_NAME::EYES,
											TASK_NAME::IDLER, EVENT_ACTION_SETVALUE,
											0, 0, nullptr );
		SwitchBoard::send (msg );
#endif

#ifdef ENABLE_JAW
		msg = Message::create_message (TASK_NAME::JAW,
											TASK_NAME::IDLER, EVENT_ACTION_SETVALUE,
											0, 0, nullptr );
		SwitchBoard::send(msg);
#endif
		ESP_LOGI("main", "Finished playing file\n" );
		fclose (fp );
	}  // END of WAITING TO START READING THE FILE
	// ESP_LOGD(TAG, "----- OOPS - should not return!------");
	return;
}

/**
 * This does a short test of the jaw motion and the eyes.
 *
 */
void SndPlayer::testEyesAndJaws ()
{
#if (defined ENABLE_JAW) | (defined ENABLE_EYER)
	Message *msg;
#endif

#if defined ENABLE_EYE
	msg = Message::create_message (TASK_NAME::EYES, TASK_NAME::IDLER,
	EVENT_ACTION_SETVALUE, 0, 255 , nullptr);
	SwitchBoard::send (msg ); // Left Eye
#endif
#if defined ENABLE_JAW
	msg = Message::create_message (TASK_NAME::JAW, TASK_NAME::IDLER,
	EVENT_ACTION_SETVALUE, 0, 0, nullptr);
	SwitchBoard::send (msg ); // Open JAW
#endif

#if (defined ENABLE_JAW) | (defined ENABLE_EYER)
	vTaskDelay (3000 / portTICK_PERIOD_MS );
#endif

#if defined ENABLE_EYE
	msg = Message::create_message (TASK_NAME::EYES, TASK_NAME::IDLER,
	EVENT_ACTION_SETVALUE, 255, 0, nullptr );
	SwitchBoard::send (msg );  // Right EYE
#endif
#if defined ENABLE_JAW
	msg = Message::create_message (TASK_NAME::JAW, TASK_NAME::IDLER,
	EVENT_ACTION_SETVALUE, 1000, 0, nullptr );
	SwitchBoard::send (msg );  // Close JAW
#endif

#if (defined ENABLE_JAW) | (defined ENABLE_EYER)
	vTaskDelay (3000 / portTICK_PERIOD_MS );
#endif

	ESP_LOGD(TAG, "NOW TO BEGIN..." );


#if defined ENABLE_EYE
	msg = Message::create_message (TASK_NAME::EYES, TASK_NAME::IDLER,
	EVENT_ACTION_SETVALUE, 0, 0, nullptr );
	SwitchBoard::send (msg );
#endif
#if defined ENABLE_JAW
	msg = Message::create_message (TASK_NAME::JAW, TASK_NAME::IDLER,
	EVENT_ACTION_SETVALUE, 0, 0, nullptr );
	SwitchBoard::send (msg );  // Close JAW
#endif
}


/**
 * This performs some initialization,
 *    does a short test to check the eyes and jaw,
 *    starts the player task running, and returns.
 * It takes care of the initialization stuff.
 */
void SndPlayer::startPlayerTask (void *_me)
{
	I2SOutput2 *output;
	SndPlayer *me = (SndPlayer*) _me;
	me->runState = PLAYER_IDLE;

	// setup the button to trigger playback - see config.h for settings
	gpio_set_direction (GPIO_BUTTON, GPIO_MODE_INPUT );
	gpio_set_pull_mode (GPIO_BUTTON, GPIO_PULLUP_ONLY );

	// Register this driver.
	SwitchBoard::registerDriver (TASK_NAME::WAVEFILE, me);

	// create the output
	output = new I2SOutput2(8000, I2S_DATA_BIT_WIDTH_16BIT);

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

void SndPlayer::dump_header(void *info_ptr )
{
	mp3dec_frame_info_t *info=(mp3dec_frame_info_t *)info_ptr;
	// frame_bytes, frame_offset, channels, hz, layer, bitrate_kbps
	ESP_LOGI(TAG, "Frame Bytes  : %d", info->frame_bytes);
	ESP_LOGI(TAG, "Frame Offset : %d", info->frame_offset);
	ESP_LOGI(TAG, "channels     : %d", info->channels);
	ESP_LOGI(TAG, "hz           : %d", info->hz);
	ESP_LOGI(TAG, "later        : %d", info->layer);
	ESP_LOGI(TAG, "Bitrate_kbps : %d", info->bitrate_kbps);
}
