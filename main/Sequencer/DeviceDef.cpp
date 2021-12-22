/**
 * DeviceDef.cpp
 *
 *  Created on: Sep 28, 2021
 *      Author: doug
 *
 */

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "DeviceDef.h"
#include "Message.h"

static const  char *TAG="DeviceDef";

DeviceDef::DeviceDef (const char *name)
{
	// TODO Auto-generated constructor stub
	ESP_LOGD(TAG, "In DeviceDef - define device %s", name);
	devName=strdup(name);

}

DeviceDef::~DeviceDef ()
{
	// TODO Auto-generated destructor stub
}
