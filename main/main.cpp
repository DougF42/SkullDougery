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
#include "Parameters/RmNvs.h"
#include "PwmDriver.h"
#include "SndPlayer.h"
#include "Network/WiFiHub.h"
#include "Stepper/StepperDriver.h"

#define ENABLE_PWM_DRIVER
#define ENABLE_WIFI
#define RUNSTEPPER

#include "audio/minimp3.h"

static const char *TAG = "MAIN::";

extern "C"
{
void app_main ();
}

void app_main ()
{
	// Configure the 'RESET' button.
	gpio_config_t io_conf = { };
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT;
	//bit mask of the pins that you want to set,e.g.GPIO18/19
	io_conf.pin_bit_mask = (1ULL << RESET_SWITCH);
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	// pull-up
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

	gpio_config (&io_conf );

	// Initialize NVS, and load parameters.
	esp_err_t ret = nvs_flash_init ();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase () );
		ret = nvs_flash_init ();
	}
	ESP_ERROR_CHECK(ret );

	// Check reset switch - if on, reset NVS parameters.
	if (1 == gpio_get_level (RESET_SWITCH ))
	{
		ESP_LOGD(TAG, "*********RESET switch not active********" );
		RmNvs::init (0 );
	}
	else
	{
		ESP_LOGD(TAG, "********RESET switch is activated********" );
		RmNvs::init (1 );
	}

	RmNvs::dumpTable ();

	TaskHandle_t switchboardHandle;

// Start Switchboard first
	ESP_LOGD(TAG, "About to start Switchboard!" );
	xTaskCreatePinnedToCore (SwitchBoard::runDelivery, "SwitchBoard", 8192,
			nullptr, 2, &switchboardHandle, ASSIGN_SWITCHBOARD_CORE );
	ESP_LOGD(TAG, "SWITCHBOARD INITIALIZED!\n" );

#ifdef ENABLE_WIFI
	WiFiHub wifi; // Define wifi instance

	if (RmNvs::is_set (RMNVS_FORCE_STA_MODE ))
	{
		ESP_LOGI(TAG, "****Starting Station mode (connect to Access Point)");
		wifi.WiFi_STA_init ();
	}
	else
	{
		// Start Network (Access Point)
		ESP_LOGI(TAG, "****Starting HUB");
		wifi.WiFi_HUB_init ();
	}
#endif
//while(1) {
//	ESP_LOGD(TAG, "....SLEEP....");
//	vTaskDelay(10000); // Sleep forever
//}

	SndPlayer player ("Player" );

	xTaskCreatePinnedToCore (player.startPlayerTask, "Player", 32768, &player,
			2, &(player.myTask), ASSIGN_SWITCHBOARD_CORE );
#ifdef RUNSTEPPER
	StepperDriver stepper("Stepper");
	xTaskCreatePinnedToCore( StepperDriver::runTask, "Stepper Task", 8192,
			&stepper, 1, nullptr, ASSIGN_STEPPER_CORE);
#endif
	while (1)
	{
		vTaskDelay (5000 / portTICK_PERIOD_MS ); // Keep me alive
	}
}
