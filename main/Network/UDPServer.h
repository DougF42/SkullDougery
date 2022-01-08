/**
 * UDPServer.h
 *
 *  Created on: Jan 6, 2022
 *      Author: doug
 *
 */

#ifndef MAIN_NETWORK_UDPSERVER_H_
#define MAIN_NETWORK_UDPSERVER_H_


#include "../config.h"
#include "../CmdDecoder.h"

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



class UDPServer: public CmdDecoder
{
public:
	UDPServer (TASK_NAME devId);
	~UDPServer();
	static void startListenTask(void *param);
	void drop_connection();

private:
	int sock;
	void UDP_Server_handleCommmands ();
	void postResponse(const char *respTxt, responseStatus_t respcode) ;
	struct sockaddr_storage source_addr; // Who sent us a message?
};

#endif /* MAIN_NETWORK_UDPSERVER_H_ */
