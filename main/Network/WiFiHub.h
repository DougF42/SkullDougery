/**
 * WiFiHub.h
 *
 *  Created on: Dec 27, 2021
 *      Author: doug
 *
 * This creates a stand-alone hub
 */

#ifndef MAIN_NETWORK_WIFIHUB_H_
#define MAIN_NETWORK_WIFIHUB_H_

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "UDPServer.h"
#include "../Sequencer/Message.h"

class WiFiHub
{
public:
	WiFiHub ();
	virtual ~WiFiHub ();
	void WiFi_HUB_init();
	void WiFi_STA_init ();

private:
	static void wifi_event_handler(void* arg, esp_event_base_t event_base,
	                                    int32_t event_id, void* event_data);
	static void UDP_Server_wait_connection (void *Parameters);
	void UDP_Server_handleCommmands();
	TaskHandle_t udpServerTask;
	UDPServer *udpserver;

};

#endif /* MAIN_NETWORK_WIFIHUB_H_ */
