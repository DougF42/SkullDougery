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
#include "Network/WiFiHub.h"

#define ENABLE_PWM_DRIVER
#define ENABLE_WIFI
#include "audio/minimp3.h"

static const char *TAG = "MAIN::";

extern "C"
{
void app_main ();
}


void app_main ()
{
	// Initialize NVS for everyone...
	esp_err_t ret = nvs_flash_init ();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase () );
		ret = nvs_flash_init ();
	}
	ESP_ERROR_CHECK(ret );
	// TODO: Start and load NVS default params here

#ifdef ENABLE_WIFI

	WiFiHub wifi(TASK_NAME::UDP);
	// TODO: SELECT BETWEEN HUB and STAtion modes???
	// Start Network (Access Point)
	wifi.WiFi_HUB_init();
	// wifiEiFi_STA_init(); ???
#endif


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
