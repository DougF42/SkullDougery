#include "SPIFFS.h"
#include <stdlib.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <errno.h>
#include <string.h>

#include "Sequencer/Message.h"
#include "Sequencer/SwitchBoard.h"
#include "config.h"
#include "PwmDriver.h"
#include "SndPlayer.h"

#define ENABLE_PWM_DRIVER

#include "audio/minimp3.h"

static const char *TAG = "MAIN::";

extern "C"
{
void app_main ();
}


void app_main ()
{

	TaskHandle_t switchboardHandle;

	// Start Switchboard first
	ESP_LOGD(TAG, "About to start Switchboard!" );
	xTaskCreatePinnedToCore (SwitchBoard::runDelivery, "SwitchBoard", 8192,
			nullptr, 2, &switchboardHandle, ASSIGN_SWITCHBOARD_CORE );
	ESP_LOGD(TAG, "SWITCHBOARD INITIALIZED!\n" );

	SndPlayer player("Player");
	//player.startPlayerTask();
	xTaskCreatePinnedToCore (player.startPlayerTask, "Player", 32768,
			&player, 2, &(player.myTask), ASSIGN_SWITCHBOARD_CORE );

	while (1)
	{
		vTaskDelay (5000 / portTICK_PERIOD_MS ); // Keep me alive
	}
}
