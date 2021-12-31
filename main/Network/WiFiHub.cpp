/**
 * WiFiHub.cpp
 *
 *  Created on: Dec 27, 2021
 *      Author: doug
 *
 */

#include "WiFiHub.h"

/*
 * WiFi_HUB.c
 *
 *  Created on: Aug 18, 2021
 *      Author: doug
 */
#include <string>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "../config.h"
#include "WiFiHub.h"
static const char *TAG = "wifi softAP";

/**
 * Watch for events...
 */
void WiFiHub::wifi_event_handler (void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data)
{
	std::string fmat = "***station %s %02x:%02x:%02x:%02x:%02x:%02x AID=%d ****";
	fmat.append ("%02x:%02x:%02x:%02x:%02x:%02x" );
	fmat.append (" AID=%d ****" );
	if (event_id == WIFI_EVENT_AP_STACONNECTED)
	{
		wifi_event_ap_staconnected_t *event =
				(wifi_event_ap_staconnected_t*) event_data;
		ESP_LOGI(TAG,
				"***station JOIN %02x:%02x:%02x:%02x:%02x:%02x AID=%d ****",
				MAC2STR(event->mac), event->aid );

	}
	else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
	{
		wifi_event_ap_stadisconnected_t *event =
				(wifi_event_ap_stadisconnected_t*) event_data;
		ESP_LOGI(TAG,
				"***station LEAVE %02x:%02x:%02x:%02x:%02x:%02x AID=%d ****",
				MAC2STR(event->mac), event->aid );
	}

}

/**
 * Set up the server, listen for UDP input.
 *
 */
void WiFiHub::UDP_Server (void *parameters)
{
	WiFiHub *me = (WiFiHub*) parameters;
	char rx_buffer[128];
	char addr_str[128];
	int addr_family = AF_INET;
	int ip_protocol = 0;

	while (1)
	{
		struct sockaddr_in dest_addr;
		dest_addr.sin_addr.s_addr = htonl(INADDR_ANY );
		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons(SKULL_WIFI_PORT );
		ip_protocol = IPPROTO_IP;
		me->sock = socket (addr_family, SOCK_DGRAM, ip_protocol );
		if (me->sock < 0)
		{
			ESP_LOGE(TAG, "Unable to create socket: errno %d", errno );
			break;
		}
		ESP_LOGI(TAG, "Socket created" );

		int err = bind (me->sock, (struct sockaddr*) &dest_addr,
				sizeof(dest_addr) );
		if (err < 0)
		{
			ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno );
		}
		ESP_LOGI(TAG, "Socket bound, port %d", SKULL_WIFI_PORT );

		// The actual server...
		while (1)
		{
			ESP_LOGI(TAG, "Waiting for data" );
			socklen_t socklen = sizeof(source_addr);
			int len = recvfrom (me->sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
					(struct sockaddr*) &me->source_addr, &socklen );

			// Error occurred during receiving
			if (len < 0)
			{
				ESP_LOGE(TAG, "recvfrom failed: errno %d", errno );
				break;
			}
			// Data received
			else
			{
				// Get the sender's ip address as string
				if (me->source_addr.ss_family == PF_INET)
				{
					inet_ntoa_r(((struct sockaddr_in* )&me->source_addr)->sin_addr,
							addr_str, sizeof(addr_str) - 1 );
				}


				rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
				ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str );
				ESP_LOGI(TAG, "%s", rx_buffer );
				// PROCESS COMMAND. Response will be generated before 'addToBuffer' returns...

				me->addToBuffer (rx_buffer, len);
				me->addToBuffer ("\n", 1);
				me->flush();


			}
		}  // End of inner while(1)

		// Must have seen an error... shut down the socket and try again.
		if (me->sock != -1)
		{
			ESP_LOGE(TAG, "Shutting down socket and restarting..." );
			shutdown (me->sock, 0 );
			close (me->sock );
		}
	}  // End of outer while(1)
	vTaskDelete (NULL );
}

/**
 * This sends a response to the current source_addr, on 'sock' (the socket).
 * @param respTxt - the text of the message to send.
 * @param respcode - A short code of responseStatus_t type that
 *       describes the nature of the error.
 *
 */
void WiFiHub::postResponse(const char *respTxt, enum responseStatus_t respcode) {
	int err = sendto (sock, respTxt, strlen(respTxt), 0, (struct sockaddr*) &source_addr,
			sizeof(source_addr) );

	if (err < 0)
	{
		ESP_LOGE(TAG, "Error occurred while trying to send a reply!  errno=%d", errno );

	}
}

/**
 * Set up the Access Point
 */
void WiFiHub::WiFi_HUB_init (void)
{
	ESP_ERROR_CHECK(esp_netif_init () );
	ESP_ERROR_CHECK(esp_event_loop_create_default () );
	esp_netif_create_default_wifi_ap ();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
	;
	ESP_ERROR_CHECK(esp_wifi_init (&cfg ) );

	ESP_ERROR_CHECK(
			esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL) );

	wifi_config_t wifi_config;
	strcpy ((char*) wifi_config.ap.ssid, SKULL_WIFI_SSID );
	wifi_config.ap.ssid_len = strlen (SKULL_WIFI_SSID );
	wifi_config.ap.channel = SKULL_WIFI_CHANNEL;
	strcpy ((char*) wifi_config.ap.password, SKULL_WIFI_PASS );
	wifi_config.ap.max_connection = 5;
	wifi_config.ap.authmode = SKULL_WIFI_AUTHMODE;

	if (strlen (SKULL_WIFI_PASS ) == 0)
	{
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode (WIFI_MODE_AP ) );
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start () );

	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
			SKULL_WIFI_SSID, SKULL_WIFI_PASS, SKULL_WIFI_CHANNEL );
}

