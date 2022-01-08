/**
 * WiFiHub.cpp
 *
 *  Created on: Dec 27, 2021
 *      Author: doug
 *
 *   This class will either (a) set up a hub others can connect to,
 *   OR (b) it connects to an existing hub. It then implements a USB
 *   command server.
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
#include "../Parameters/RmNvs.h"
#include "UDPServer.h"
static const char *TAG = "wifi softAP";


//TaskHandle_t WiFiHub::udpServerTask;
//UDPServer WiFiHub::udpsever(TASK_NAME::UDP);


/**
 * Watch for events...
 *  NOTE that this listener handles BOTH the HUB and the Access Point modes!!!
 */
void WiFiHub::wifi_event_handler (void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data)
{
	WiFiHub *me=(WiFiHub *)arg;
	wifi_event_ap_staconnected_t *event_connect;
	wifi_event_ap_stadisconnected_t *event_disc;
	wifi_event_sta_connected_t *event_connect_sta;
	std::string fmat = "***station %s %02x:%02x:%02x:%02x:%02x:%02x AID=%d ****";
	fmat.append ("%02x:%02x:%02x:%02x:%02x:%02x" );
	fmat.append (" AID=%d ****" );

	// Events as a HUB
	if (event_base == WIFI_EVENT)
	{
		switch (event_id)
		{
			case (WIFI_EVENT_AP_STACONNECTED):
				// Acting as a hub, someone connected to us.
				event_connect = (wifi_event_ap_staconnected_t*) event_data;
				ESP_LOGI(TAG,
						"***Station JOIN %02x:%02x:%02x:%02x:%02x:%02x AID=%d ****",
						MAC2STR(event_connect->mac), event_connect->aid );
				break;

			case (WIFI_EVENT_AP_STADISCONNECTED):
				// Acting as a hub, someone disconnected from us.
				event_disc = (wifi_event_ap_stadisconnected_t*) event_data;
				ESP_LOGI(TAG,
						"***Station LEAVE %02x:%02x:%02x:%02x:%02x:%02x AID=%d ****",
						MAC2STR(event_disc->mac), event_disc->aid );
				break;

			case (WIFI_EVENT_STA_START):
				// WIFI has started - now connect to the network hub.
				esp_wifi_connect ();
				break;

			case (WIFI_EVENT_STA_CONNECTED):
				// TODO: WiFi has connected to the HUB, but does not yet have DHCP address.
				event_connect_sta = (wifi_event_sta_connected_t*) event_data;
				char ssid[9];
				memcpy (ssid, event_data, event_connect_sta->ssid_len );
				ssid[event_connect_sta->ssid_len] = '\0';
				ESP_LOGI(TAG, "CONNECTED to HUB: ssid=%s,  channel=%d", ssid,
						event_connect_sta->channel );

				// Time to force addreess??
				break;

			case (WIFI_EVENT_STA_DISCONNECTED):
				// Something happend - we disconnected from hub, or failed to connect
				ESP_LOGI(TAG, "DISCONNECTED FROM HUB (event)" );
				// For now, shut down the udp server.
				vTaskDelete(me->udpServerTask);

				// Probably need to keep trying
				break;

		} // End of switch for WiFi events
	}   // End of IF WIFI event


	if (event_base == IP_EVENT)
	{
		switch (event_id)
		{

			case (IP_EVENT_STA_GOT_IP):
				// TODO: start the server
				ESP_LOGI(TAG, "STATION MODE: Got IP address: " );
				if (me->udpServerTask==nullptr) {
					me->udpserver=new UDPServer(TASK_NAME::UDP);
					xTaskCreate (UDPServer::startListenTask, "UDP Server", 8192,
							(void*) me->udpserver, 1, &me->udpServerTask );
				}
				break;

			case (IP_EVENT_STA_LOST_IP):
				// TODO:
				vTaskDelete(me->udpserver);
				delete me->udpserver;
				break;

		} // end of case for IP events
	} // end of IF for IP EVENTs
}

/**
 * Shut it down... ?
 */
WiFiHub::~WiFiHub() {

}


/**
 * Initializer - do nothing.
 */
 WiFiHub::WiFiHub() {

}


/**
 * Set up the Access Point
 */
void WiFiHub::WiFi_HUB_init ()
{


	ESP_ERROR_CHECK(esp_netif_init () );
	ESP_ERROR_CHECK(esp_event_loop_create_default () );
	esp_netif_create_default_wifi_ap ();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
	;
	ESP_ERROR_CHECK(esp_wifi_init (&cfg ) );

	ESP_ERROR_CHECK(
			esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, this, NULL ));

	ESP_ERROR_CHECK(
			esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, this, NULL ));

	ESP_LOGI(TAG, "wifi_init_softap started. SSID:%s password:%s channel:%d",
			RmNvs::get_str(RMNVS_KEY_WIFI_SSID),
			RmNvs::get_str(RMNVS_KEY_WIFI_PASS),
			RmNvs::get_int(RMNVS_WIFI_CHANNEL) );

	wifi_config_t wifi_config;
	strcpy ((char*) wifi_config.ap.ssid,
			RmNvs::get_str (RMNVS_KEY_WIFI_SSID ) );
	wifi_config.ap.ssid_len = strlen (RmNvs::get_str (RMNVS_KEY_WIFI_SSID ) );
	wifi_config.ap.channel = RmNvs::get_int (RMNVS_WIFI_CHANNEL );
	strcpy ((char*) wifi_config.ap.password,
			RmNvs::get_str (RMNVS_KEY_WIFI_PASS ) );
	wifi_config.ap.max_connection = 5;
	wifi_config.ap.authmode = SKULL_WIFI_AUTHMODE;

	if (strlen (RmNvs::get_str (RMNVS_KEY_WIFI_PASS ) ) == 0)
	{
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode (WIFI_MODE_AP ) );
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start () );

	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
			RmNvs::get_str(RMNVS_KEY_WIFI_SSID),
			RmNvs::get_str(RMNVS_KEY_WIFI_PASS),
			RmNvs::get_int(RMNVS_WIFI_CHANNEL) );
	udpserver= new UDPServer(TASK_NAME::UDP);
	xTaskCreate (UDPServer::startListenTask, "UDP Server", 8192,
			(void*) udpserver, 1, &udpServerTask );
}


/**
 * This connects to an external access point as a 'station'
 *
 */
void WiFiHub::WiFi_STA_init ()
{
	UDPServer udpsever (TASK_NAME::UDP );
	ESP_ERROR_CHECK(esp_netif_init () );

	ESP_ERROR_CHECK(esp_event_loop_create_default () );
	esp_netif_create_default_wifi_sta ();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
	;
	ESP_ERROR_CHECK(esp_wifi_init (&cfg ) );

	ESP_ERROR_CHECK(
			esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, nullptr) );

	ESP_ERROR_CHECK(
			esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, nullptr) );

	wifi_config_t wifi_config =
	{ };
	// wifi_config.sta.ssid = RmNvs::get_str(RMNVS_KEY_WIFI_SSID);
	strcpy ((char*) wifi_config.sta.ssid,
			RmNvs::get_str (RMNVS_KEY_WIFI_SSID ) );
	//wifi_config.sta.password = RmNvs::get_str(RMNVS_KEY_WIFI_PASS);
	strcpy ((char*) wifi_config.sta.password,
			RmNvs::get_str (RMNVS_KEY_WIFI_PASS ) );
	/* Setting a password implies station will connect to all security modes including WEP/WPA.
	 * However these modes are deprecated and not advisable to be used. In case your Access point
	 * doesn't support WPA2, these mode can be enabled by commenting below line */
	wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
	wifi_config.sta.pmf_cfg.capable = true;
	wifi_config.sta.pmf_cfg.required = false;

	ESP_ERROR_CHECK(esp_wifi_set_mode (WIFI_MODE_STA ) );
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start () );

	// Note: We can't start the UDP listener until we get an IP address.
}
