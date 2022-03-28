/**
 * UDPServer.cpp
 *
 *  Created on: Jan 6, 2022
 *      Author: doug
 *
 * This creates a socket and listens for a UDP command.
 * The address and socket are retrieved from the RmNvs system.
 * If we disconnect from the hub, the WiFi class will call
 * vTaskDelete on us.
 */

#include "../config.h"
#include "UDPServer.h"
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

#include "../Parameters/RmNvs.h"
#include "../Sequencer/DeviceDef.h"
#include "../Sequencer/SwitchBoard.h"

const static char *TAG="...UDPSERVER";

UDPServer::UDPServer (TASK_NAME devId):CmdDecoder (devId)
{
	sock=-1;
}

UDPServer::~UDPServer() {
	if (sock >=0 ) close(sock);
}

/**
 * Open the receiver socket, listen for incoming commands
 * and process them. Stop if we see an error or disconnect.
 * @param  pointer to this instance of UDPServer.
 */
void UDPServer::startListenTask(void *param)
{
	UDPServer *me= (UDPServer *)param;

	int addr_family = AF_INET;
	int ip_protocol = 0;

	ESP_LOGI(TAG, "Starting UDP commands server - port %d", RmNvs::get_int(RMNVS_CMD_PORT));

	//TODO: REGISTER AS A DEVICE (to receive responses!)
	SwitchBoard::registerDriver(TASK_NAME::UDP, me);

	while (1)
	{
		struct sockaddr_in dest_addr;
		dest_addr.sin_addr.s_addr = htonl(INADDR_ANY );
		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons( RmNvs::get_int(RMNVS_CMD_PORT) );
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
		ESP_LOGI(TAG, "Socket bound, port %d",  ntohs(dest_addr.sin_port) );

		me->UDP_Server_handleCommmands( );

		// Must have seen an error or disconnect... shut down the socket and try again.
		if (me->sock != -1)
		{
			ESP_LOGE(TAG, "Shutting down socket..." );
			shutdown (me->sock, 0 );
			close (me->sock );
		}

	}  // End of outer while(1)

}


/*
 * Data comes from any source (no concept of a connection), we process each
 *  command as soon as it is recevied and send a reply to the source.
 * Each packet is a complete command.
 */
void UDPServer::UDP_Server_handleCommmands() {
	char rx_buffer[128];
	char addr_str[128];
	// The actual server...
	while (1) {
		ESP_LOGI(TAG, "Waiting for data");
		socklen_t socklen = sizeof(source_addr);
		int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
				(struct sockaddr*) &source_addr, &socklen);

		// Error occurred during receiving
		if (len < 0) {
			ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
			break;

		} else {    // Data received
					// Get the sender's ip address as string
			ESP_LOGI(TAG, "Received data...");
			if (source_addr.ss_family == PF_INET) {
				inet_ntoa_r(((struct sockaddr_in* )&source_addr)->sin_addr,
						addr_str, sizeof(addr_str) - 1);
			} else {
				ESP_LOGE(TAG,
						"OOPS - address family is not PF_INET in USPServer.cpp");
			}

			rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
			ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
			ESP_LOGI(TAG, "%s", rx_buffer);
			// PROCESS COMMAND. Response will be generated before 'addToBuffer' returns...
			addToBuffer(rx_buffer, len);
			addEOLtoBuffer();
		}
	}  // End of inner while(1)

}


/**
 * This formats any response message and sends it over the network to the current source_addr,
 * on 'sock' (the UDP socket).
 *
 * @param respTxt - the text of the message to send.
 *
 * @param respcode - A short code of responseStatus_t type that
 *       describes the nature of the error.
 *
 */
void UDPServer::postResponse (const char *respTxt, responseStatus_t respcode)
{
	char tmpbuf[128];
	int txtLen=strlen(respTxt)+2;
	if (txtLen >= sizeof(tmpbuf)) {
		ESP_LOGE(TAG, "postResponse: text is %d bytes - max is %ud", txtLen, sizeof(tmpbuf));
		return;
	}

	tmpbuf[0] = '\0';
	strcpy (tmpbuf, respTxt );
	strcat (tmpbuf, "\n");


	//ESP_LOGD(TAG, "POST RESPONSE: %s", tmpbuf);
	int err = 0;
	while (err <= 0)
	{
		err = sendto (sock, tmpbuf, txtLen, 0,
				(struct sockaddr*) &source_addr, sizeof(source_addr) );
		vTaskDelay(1); // Allow wifi to run

		if (err < 0)
		{
			ESP_LOGE(TAG,
					"Error occurred while trying to send %d byte reply!  errno=%d (%s)",
					txtLen, errno, strerror(errno) );
			vTaskDelay(1000);
			break;
		}
	}
}
