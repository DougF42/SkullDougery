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
 * KEYS can not be more than 32 characters.
 *
 * The known datatypes:
 *     STRING can not be more than 32 chars.
 *     INT    is really singed 32 bit.
 *     ADDR   is unsigned 32 bit, address is always in network-byte-order
 *     BOOL   is actually an integer, and follows C rules (0 if false, any value is true)
 */
enum RMNVS_DTA_t { RMNVS_STRING, RMNVS_INT, RMNVS_ADDR, RMNVS_BOOL,  RMNVS_END };

// This defines the configuration settings that can be accessed.
// NOTE: New settings must also be defined in RMNVS_init_values.
#define RMNVS_KEY_WIFI_SSID "ssid"
#define RMNVS_KEY_WIFI_PASS "pass"
#define RMNVS_WIFI_CHANNEL  "channel"
#define RMNVS_FORCE_STA_MODE  "STAmode"

#define RMNVS_IP            "addr"
#define RMNVS_NETMASK       "mask"
#define RMNVS_CMD_PORT      "cmdport"


#define RMNVS_USE_DHCP       "dhcp"
#define RMNVS_ROUTER_ADDR   "router"
#define RMNVS_SRV_ADDR      "srvaddr"
#define RMNVS_SRV_PORT      "srvport"
#define RMNVS_DNS_ADDR      "dns"
#define RMNVS_NOD_MIN_POS   "nodMinPos"
#define RMNVS_NOD_MAX_POS   "nodMaxPos"
#define RMNVS_ROT_MIN_POS   "rotMinPos"
#define RMNVS_ROT_MAX_POS   "rotMaxPos"

class RmNvs
{
public:

	/*
	 * The class methods and members are static,
	 * so we wont need to implement it in each
	 * and every function that needs a parameter...
	 */
	static void init (bool forceReset);

	static void commit ();
	static void clear_nvs ();

	static const char* get_str (const char *key);
	static int get_int (const char *key);
	static in_addr_t get_addr(const char *key);
	static int get_addr_as_string(const char *key, char *buf);
	static bool is_set(const char *key);

// Change a setting. Does NOT commit the change
	static int set_str(const char *key, const char *value);
	static int set_int (const char *key, int32_t value);
	static int set_addr (const char *key, in_addr_t value);
	static int set_addr_as_string (const char *key, const char *value);
	static int set_bool(const char *key, bool value);

	RMNVS_DTA_t get_info (int idx, bool **changeFlag, const char **keyName,
			const void **dta);
	static const char *get_info(int idx);
	static void dumpTable();
private:
	// TODO: Later:   bool getIdx (int idx, CMDMSGPTR msg);
	static void initSingleString(int idx,const  char *key, const char *val);
	static void initSingleInt(int idx, const char *key, int32_t val);
	static void initSingleAddr( int idx, const char *key, const char *addrStr);
	static void initSingleBool( int idx, const char *key, bool val);
	static void init_values();
	static void load_from_nvs();
	static int findKey(const char *keyname);
};

#endif /* MAIN_NVS_ACCESS_H_ */
