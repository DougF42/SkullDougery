/*
 * nvs_access.h
 *
 *  Created on: Jun 3, 2021
 *      Author: doug
 */

#ifndef MAIN_NVS_ACCESS_H_
#define MAIN_NVS_ACCESS_H

#include "esp_netif.h"

/**
 * The known datatypes:
 */
enum RMNVS_DTA_t { RMNVS_STRING, RMNVS_INT, RMNVS_END };

// This defines the configuration settings that can be accessed.
// NOTE: New settings must also be defined in RMNVS_init_values.
#define RMNVS_KEY_WIFI_SSID "ssid"
#define RMNVS_KEY_WIFI_PASS "pass"
#define RMNVS_WIFI_CHANNEL  "CHANNEL"
#define RMNVS_FORCE_AP_MODE  "APmode"
#define RMNVS_USE_DHCP       "dhcp"
#define RMNVS_IP            "addr"
#define RMNVS_NETMASK       "mask"
#define RMNVS_ROUTER_ADDR   "router"
#define RMNVS_CMD_PORT      "cmdport"
#define RMNVS_SRV_ADDR      "srvaddr"
#define RMNVS_SRV_PORT      "srvport"
#define RMNVS_DNS_ADDR      "dns"

class RmNvs
{
	/*
	 * The class methods and members are static,
	 * so we wont need to implement it in each
	 * and every function that needs a parameter...
	 */
	static void init (bool forceReset);

	static void commit ();
	static void clear_nvs ();
	static bool nvs_isSet ();

	static const char* get_str (const char *key);
	static int get_number (const char *key);
	static int get_addr(const char *key);

// Change a setting. Does NOT commit the change
	static void set_str(const char *key, const char *value);
	static void set_number (const char *key, int32_t value);
	static void set_addr (const char *key, int value);

	RMNVS_DTA_t get_info (int idx, bool *changeFlag, const char *keyName,
			const void *dta);
private:
	// TODO: Later:   bool getIdx (int idx, CMDMSGPTR msg);
	static void initSingleString(int idx,const  char *key, const char *val);
	static void initSingleInt(int idx, const char *key, int32_t val);
	static void init_values();
	static void load_from_nvs();
	static int findKey(const char *keyname);
};

#endif /* MAIN_NVS_ACCESS_H_ */
