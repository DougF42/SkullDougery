/*
 *
 * nvsAccess.c
 *   PRovide routines to access NVS - save and restore data.
 *
 *  The version of the nvs data is indicated by the RMNVS_VERSION_KEY.
 *  it is integer, and incremented by the programer if the NVS storage
 *  format changes in a maner incompatible with older versions. In that
 *  case, we clear the NVS regardless of any content.
 *
 *  Created on: Jun 2, 2021
 *      Author: doug
 *
 *  About IP addresses:
 *      The current code only does IPV4, adresses are 4 bytes (uint32_t).
 *      I got tired of tracking where various address conversion routines, so I
 *         have encapsulated them here.
 */
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "nvs.h"
#include "RmNvs.h"
#include "../config.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define NVS_VERSION "1.0"




static bool have_init_ok = false;
static nvs_handle_t handle;
#define MAX_VALUES 15
static const char * NVS_PREFIX = "REMOTE_MOD";
static const char *TAG         = "----NVS_ACCESS:";


#define EQUALSTR(_a_, _b_) (0==strcasecmp(_a_,_b_))

#define RMVS_END "END"


struct curValues_t {
	bool changed;           // If non-zero, this value was changes since last commmit.
	char keyName[15];       // Name of this key. No more than 15 chars
	RMNVS_DTA_t  datatype;  // How was this stored?
	union {
		char curString[32]; // Current value of this key (if string). Not longer than 32 chars
		int32_t curNumber;  // Current value of this key (if integer).
		in_addr_t curAddr;   // Current value of this key (if an address).
		int32_t  curBool;   // Current value of this key (if boolean)
	};
} ;

static struct curValues_t curValues[15];
static int NOOFCURVALUES;    // Filled in by init


/**
 * This sets individual string defaults - it is ONLY used  by RMNVS_init_values.
 * As such, there should NEVER be any failure here - if failure, we panic and abort.
 */
void RmNvs::initSingleString(int idx, const char *key, const char *val) {
	if (idx > sizeof(curValues)/sizeof(struct curValues_t)) {
		ESP_LOGE(TAG, "In RMNVS: initSingleValues. Index is %d, larger than limit of %d",
				idx, sizeof(curValues)/sizeof(struct curValues_t));
		abort();
	}

	curValues[idx].changed=true;
	if (strlen(key) > (sizeof(curValues[idx].keyName)-1)) {
		ESP_LOGE(TAG, "In RMNVS: initSingleValues. Key name %s is %d chars long, max is %d",
				key, strlen(key), (sizeof(curValues[idx].keyName)-1));
		abort();
	}
	strcpy( curValues[idx].keyName, key);

	if (strlen(val) > (sizeof(curValues[idx].curString)-1)) {
		ESP_LOGE(TAG, "In RMNVS: initSingleValues. Value for key %s is %s: this is %d chars long, max is %d",
				key, val, strlen(val), (sizeof(curValues[idx].curString)-1));
		abort();
	}
	strcpy( curValues[idx].curString, val);
}

/**
 * This sets individual string defaults - it is ONLY used  by RMNVS_init_values.
 * As such, there should NEVER be any failure here - if failure, we panic and abort.
 */
void RmNvs::initSingleInt(int idx, const char *key, int val) {
	if (idx > sizeof(curValues)/sizeof(struct curValues_t)) {
		ESP_LOGE(TAG, "In RMNVS: initSingleValues. Index is %d, larger than limit of %d",
				idx, sizeof(curValues)/sizeof(struct curValues_t));
		abort();
	}

	curValues[idx].changed=true;
	if (strlen(key) > (sizeof(curValues[idx].keyName)-1)) {
		ESP_LOGE(TAG, "In RMNVS: initSingleValues. Key name %s is %d chars long, max is %d",
				key, strlen(key), (sizeof(curValues[idx].keyName)-1));
		abort();
	}
	strcpy( curValues[idx].keyName, key);

	curValues[idx].datatype = RMNVS_INT;
	curValues[idx].curNumber =  val;
}


/**
 * This sets individual string defaults - it is ONLY used  by RMNVS_init_values.
 * As such, there should NEVER be any failure here - if failure, we panic and abort.
 */
void RmNvs::initSingleBool(int idx, const char *key, bool val) {
	if (idx > sizeof(curValues)/sizeof(struct curValues_t)) {
		ESP_LOGE(TAG, "In RMNVS: initSingleValues. Index is %d, larger than limit of %d",
				idx, sizeof(curValues)/sizeof(struct curValues_t));
		abort();
	}

	curValues[idx].changed=true;
	if (strlen(key) > (sizeof(curValues[idx].keyName)-1)) {
		ESP_LOGE(TAG, "In RMNVS: initSingleValues. Key name %s is %d chars long, max is %d",
				key, strlen(key), (sizeof(curValues[idx].keyName)-1));
		abort();
	}
	strcpy( curValues[idx].keyName, key);

	curValues[idx].datatype = RMNVS_BOOL;
	if (val)
		curValues[idx].curBool = true;
	else
		curValues[idx].curBool = false;
}


/**
 * Initializes an address in the paramter table. ONLY used by RMNVS_init_values.
 * @param idx - what index to set.
 * @param key - the key to use.
 * @param addrStr - the initial address. Note that although we take the address as
 *              a string, it is actually stored network-byte-order.
 */
void RmNvs::initSingleAddr( int idx, const char *key, const char *addrStr) {
	if (idx > sizeof(curValues)/sizeof(struct curValues_t)) {
		ESP_LOGE(TAG, "In RMNVS: initSingleAddr. Index is %d, larger than limit of %d",
				idx, sizeof(curValues)/sizeof(struct curValues_t));
		abort();
	}

	curValues[idx].changed=true;
	if (strlen(key) > (sizeof(curValues[idx].keyName)-1)) {
		ESP_LOGE(TAG, "In RMNVS: initSingleValues. Key name %s is %d chars long, max is %d",
				key, strlen(key), (sizeof(curValues[idx].keyName)-1));
		abort();
	}
	strcpy( curValues[idx].keyName, key);
	curValues[idx].datatype = RMNVS_ADDR;
	inet_aton(addrStr, &curValues[idx].curAddr);

}

/**
 * This initializes the values in the curValues structure
 * NOTE: The 'RMVNS_END' paramter MUST be the last in the sequence!
 */
void RmNvs::init_values() {

	int idx=0;

#if defined NET_DOUGSHOME
	initSingleString(idx++, RMNVS_KEY_WIFI_SSID,  "Daisy-5_2GEXT");
	initSingleString(idx++, RMNVS_KEY_WIFI_PASS,  "iknowits42");
	initSingleBool  (idx++, RMNVS_FORCE_STA_MODE, true);
	initSingleAddr  (idx++, RMNVS_IP,             "192.168.4.1");  // This is my address
#elif defined NET_MAKERSLAIR
	initSingleString(idx++, RMNVS_KEY_WIFI_SSID,  "Makers'Lair-2.4");
	initSingleString(idx++, RMNVS_KEY_WIFI_PASS,  "sparksAreHere");
	initSingleBool  (idx++, RMNVS_FORCE_STA_MODE, true);
	initSingleAddr  (idx++, RMNVS_IP,             "192.168.4.1");  // This is my address
#elif defined NET_NYOWNHUB
	initSingleString(idx++, RMNVS_KEY_WIFI_SSID,  "skulldougery");
	initSingleString(idx++, RMNVS_KEY_WIFI_PASS,  "password");
	initSingleBool  (idx++,   RMNVS_FORCE_STA_MODE, false);
	initSingleAddr  (idx++, RMNVS_IP,             "192.168.4.1");  // This is my address
#endif

	initSingleInt   (idx++, RMNVS_CMD_PORT,       100);
	initSingleBool  (idx++, RMNVS_USE_DHCP,       true);
	initSingleAddr  (idx++, RMNVS_IP,             "192.168.4.1");  // This is my address
	initSingleAddr  (idx++, RMNVS_NETMASK,        "255.255.255.0");
	initSingleString(idx++, RMNVS_ROUTER_ADDR,    " ");

	initSingleAddr  (idx++, RMNVS_SRV_ADDR,       "192.168.4.4");  // Who I should log to
	initSingleInt   (idx++, RMNVS_SRV_PORT,       3001);
	initSingleAddr  (idx++, RMNVS_DNS_ADDR,       "8.8.8.8");
	initSingleInt   (idx++, RMNVS_WIFI_CHANNEL,      1);
	initSingleString(idx++, RMVS_END,             "END");
	curValues[idx].datatype=RMNVS_END;   // Force END flag.
	NOOFCURVALUES=idx;
};



/**
 * Write any 'changed' values to NVS.
 */
void RmNvs::commit ()
{
	int idx;
	int err;
	char buf[64];
	ESP_LOGD(TAG, "IN RMNVS_commit. Number of curvalues is %d", NOOFCURVALUES );
	for (idx = 0; idx < NOOFCURVALUES; idx++ )
	{
		err = ESP_OK;
		if (curValues[idx].changed)
		{
			err = ESP_OK;
			switch (curValues[idx].datatype)
			{
				case (RMNVS_STRING):

					ESP_LOGD(TAG,
							"RMNVS_commit: Processing STRING key %d name %s  value %s",
							idx, curValues[idx].keyName,
							curValues[idx].curString );
					err = nvs_set_str (handle, curValues[idx].keyName,
							curValues[idx].curString );
					break;

				case (RMNVS_INT):
					ESP_LOGD(TAG,
							"RMNVS_commit: Processing INTEGER key %d name %s  value %d",
							idx, curValues[idx].keyName,
							curValues[idx].curNumber );

					ESP_LOGD(TAG, "RMNVS_commit: saving key %s",
							curValues[idx].keyName );
					err = nvs_set_i32 (handle, curValues[idx].keyName,
							curValues[idx].curNumber );
					break;

				case (RMNVS_ADDR):
					RmNvs::get_addr_as_string (curValues[idx].keyName, buf );
					ESP_LOGD(TAG, "RMNVS_commit: saving ADDRESS key %s value %s",
							curValues[idx].keyName,	buf);
					err = nvs_set_u32 (handle, curValues[idx].keyName,
							curValues[idx].curAddr );
					break;

				case (RMNVS_BOOL):
					ESP_LOGD(TAG, "RMNVS_commit: saving key %s, value %d",
							curValues[idx].keyName, curValues[idx].curBool );
					err = nvs_set_i32 (handle, curValues[idx].keyName,
							curValues[idx].curBool );
					break;

				case (RMNVS_END):
					break;
			} // End of switch

			if (err != ESP_OK)
			{
				// Something wicked this way comes!
				ESP_LOGE(TAG,
						"In RMNVS_commit - something wicked this way comes!  Key %d",
						idx );
				ESP_LOGE(TAG, "....KEY is %s", curValues[idx].keyName );
			}
			curValues[idx].changed = false;
		} // End of if changed
	}  // End of for loop
	return;
}

/**
 * Clear NVS - this removes all configs from NVS, so on next
 * boot we will use the 'factory defaults'.
 */
void RmNvs::clear_nvs() {
	ESP_LOGD(TAG, "Now in RMNVS_clear_nvs");

	nvs_close(handle);
	ESP_ERROR_CHECK(nvs_flash_erase());
	esp_err_t err = nvs_flash_init();
	ESP_ERROR_CHECK(err);  // Report any error (should be none!)
	ESP_ERROR_CHECK(nvs_open(NVS_PREFIX, NVS_READWRITE, &handle));
	/*
	nvs_iterator_t it = nvs_entry_find("nvs", RM_NAMESPACE, NVS_TYPE_ANY);
	while (it != NULL) {
	        nvs_entry_info_t info;
	        nvs_entry_info(it, &info);
	        it = nvs_entry_next(it);
	        ESP_LOGD(TAG, "key '%s', type '%d' \n", info.key, info.type);
	};
	*/
}

/**
 * This attempts to load any values that are stored in NVS.
 * It is NOT an error if one is missing -
 */
void RmNvs::load_from_nvs ()
{
	int idx;
	int err;
	size_t len;

	for (idx = 0; idx < NOOFCURVALUES; idx++ )
	{
		err=ESP_OK;
		switch (curValues[idx].datatype)
		{
			case (RMNVS_STRING):
				len=sizeof(curValues[idx].curString);
				err = nvs_get_str (handle, curValues[idx].keyName,
						curValues[idx].curString, &len );
				break;

			case (RMNVS_INT):
				err = nvs_get_i32 (handle, curValues[idx].keyName,
						&(curValues[idx].curNumber) );
				break;

			case (RMNVS_ADDR):
				err = nvs_get_u32 (handle, curValues[idx].keyName,
						&(curValues[idx].curAddr) );
				break;

			case (RMNVS_BOOL):
				err = nvs_get_i32 (handle, curValues[idx].keyName,
						&(curValues[idx].curBool) );
				break;

			case (RMNVS_END):
				// Do nothing...
				break;
		}

		if (err == ESP_OK)
		{
			curValues[idx].changed = false;
		}
		else
		{
			ESP_LOGE(TAG, "In 'load_from_nvs: get failed for %s. ret code err %d(%s)",
					curValues[idx].keyName, err, esp_err_to_name(err));
		}
	}
	return;
}

/**
 * Initialize the NVS memory system.
 *   **** Only called once on startup. ****
 *
 *
 * @param forceReset - if true, then intialize the partition.
 */
void RmNvs::init(bool forceReset) {
	size_t flen=5;
	char  flag[flen];

	if (have_init_ok) {
		ESP_LOGE(TAG, "ERROR: Called RmNvd_init more than once???");
		return;
	}
	have_init_ok = true;
	ESP_LOGD(TAG, "In RmNvs init");

	ESP_ERROR_CHECK(nvs_open(NVS_PREFIX, NVS_READWRITE, &handle));
	init_values(); // give our in-core array its default values.
	/*
	 *  try to acces RMNVS_KEY_WIFI_SSID.
	 * If not present, set NO_CONFIG flag.
	 */
	if (forceReset  ||  ESP_OK != nvs_get_str(handle, RMVS_END, flag, &flen) ) {
		// Initialize using defaults if we dont have the RMNVS_INITIALIZED flag.
		ESP_LOGD(TAG, " Reset NVS - do not load existing NVS content");
	}  else {
		// Get any existing settings, then commit defaults for any that aren't in NVS.
		ESP_LOGD(TAG, " loading existing NVS content");
		load_from_nvs();
	}
	commit();   // Anything that wasn't read from NVS gets written now.

	ESP_LOGD(TAG, "Leave RmNvs init.");

	return;
}


/**
 * INTERNAL ONLY: Find a key in the list.
 * Return -1 if not found.
 */
int RmNvs::findKey(const char *keyname) {
	int idx;
	for (idx=0; idx<NOOFCURVALUES; idx++) {
		if (EQUALSTR( keyname, curValues[idx].keyName)) return(idx);
	}
	ESP_LOGE(TAG, "findKey failed to find key %s", keyname);
	return (-1);
}


/**
 * Get the config value from NVS  as a string, based on its key
 * We return a pointert to a CONSTANT - dont modify it, or try
 * to 'free' it.
 * @param key - the data item to read.
 * @return - pointer to the string in static memmory. DO NOT MODIFY OR FREE!
 *      nullptr if not found.
 */
const char *RmNvs::get_str(const char *key) {
	int idx=findKey(key);

	if (idx<0) {
		ESP_LOGE(TAG, "In RMNVS_get: findKey failed to find key '%s'", key);
		return(nullptr);
	}

	if (curValues[idx].datatype != RMNVS_STRING)
	{
		ESP_LOGE(TAG, "KEY %s is not a string!", key);
		return (nullptr);
	}

	return(curValues[idx].curString);
}

/**
 * Set the string value.
 * This silently truncates if the string is too long!
 */
int RmNvs::set_str (const char *key, const char *value)
{
	int idx = findKey (key );
	// dont free any longer!    free(curValues[idx].curValue);
	if (idx<0)
		{
		return(BAD_NUMBER);
		}
	strncpy (curValues[idx].curString, value,
			sizeof(curValues[idx].curString) );
	curValues[idx].curString[sizeof(curValues[idx].curString)] = '\0';
	curValues[idx].changed = true;
	return(ESP_OK);
}

/**
 * Get the requested value as an integer.
 *
 * Returns BAD_NUMBER (from messages.h) if any error
 */
int RmNvs::get_int (const char *key)
{
	int idx = findKey (key );
	if (idx < 0)
	{
		ESP_LOGE(TAG, "KEY '%s' not found in table", key);
		return (BAD_NUMBER);
	}

	if (curValues[idx].datatype != RMNVS_INT)
	{
		ESP_LOGE(TAG, "KEY '%s' is not an integer!", key);
		return (BAD_NUMBER);
	}
	return (curValues[idx].curNumber);
}

/**
 * Set a numeric value.
 *   (the value is assumed to be an int )
 *   @return - ESP_OK normally, BAD_NUMBER
 *            if the key was not found
 */
int RmNvs::set_int(const char *key, int32_t value) {
	int idx = findKey (key);
	if (idx < 0)
	{
		ESP_LOGD(TAG, "set_int::Index not found for %s", key);
		return (BAD_NUMBER);
	}

	if (curValues[idx].datatype != RMNVS_INT)
	{
		ESP_LOGD(TAG, "set_int:: datatype was %d, not %d", curValues[idx].datatype, RMNVS_INT);
		return (BAD_NUMBER);
	}
	curValues[idx].changed = true;
	curValues[idx].curNumber = value;
	return(ESP_OK);
}


/**
 * Get the address in internet byte order.
 *  This should be suitable for various network-related function calls.
 */
in_addr_t RmNvs::get_addr(const char *key) {
	int idx = findKey (key );
	if (idx < 0)
	{
		ESP_LOGE(TAG, "KEY '%s' not found in table", key);
		return (BAD_NUMBER);
	}

	if (curValues[idx].datatype != RMNVS_ADDR)
	{
		ESP_LOGE(TAG, "KEY '%s' is not an address!", key);
		return (BAD_NUMBER);
	}
	return (curValues[idx].curAddr);
}


int RmNvs::set_bool(const char *key, bool value) {
	int idx = findKey (key);
	if (idx < 0)
	{
		return (BAD_NUMBER);
	}

	if (curValues[idx].datatype != RMNVS_BOOL)
	{
		return (BAD_NUMBER);
	}
	curValues[idx].changed = true;
	curValues[idx].curNumber = value;
	return(ESP_OK);
}

/**
 * Get the requested value as an integer.
 *
 * Returns BAD_NUMBER (from messages.h) if any error
 */
bool RmNvs::is_set (const char *key)
{
	int idx = findKey (key );
	if (idx < 0)
	{
		ESP_LOGE(TAG, "KEY '%s' not found in table", key);
		return (BAD_NUMBER);
	}

	if (curValues[idx].datatype != RMNVS_BOOL)
	{
		ESP_LOGE(TAG, "KEY '%s' is not BOOLEAN!!", key);
		return (BAD_NUMBER);
	}
	return ( curValues[idx].curNumber);
}




/**
 * Get the address in string format
 * @param key - the key to look up.
 * @param buf - a buffer big enough for the resulting string (at least 16 bytes)
 *
 * @return - 0 normally, 'BAD_NUMBER' if any error.
 */
int RmNvs::get_addr_as_string(const char *key, char *buf) {
	int idx = findKey (key );
	if (idx < 0)
	{
		ESP_LOGE(TAG, "KEY '%s' not found in table", key);
		return (BAD_NUMBER);
	}

	if (curValues[idx].datatype != RMNVS_ADDR)
	{
		ESP_LOGE(TAG, "KEY '%s' is not an address!", key);
		return (BAD_NUMBER);
	}

	char *cptr=inet_ntoa(curValues[idx].curAddr);
	strcpy(buf, cptr);
	return(0);

}


/**
 * Set the paramter to the given network-byte ordered value.
 */
int RmNvs::set_addr (const char *key, in_addr_t value) {
	int idx = findKey (key);
	if (idx < 0)
	{
		return (BAD_NUMBER);
	}

	if (curValues[idx].datatype != RMNVS_ADDR)
	{
		return (BAD_NUMBER);
	}
	curValues[idx].changed = true;
	curValues[idx].curAddr = value;
	return(ESP_OK);
}

/**
 * This converts the given null-terminated string to the
 * network-byte order value (see inet_aton) and stores it
 * @oaram key - the paramter to set
 * @param value - pointer to the string to set.
 * @return the ip address as a single value, or BAD_NUMBER if any error.
 */
int RmNvs::set_addr_as_string (const char *key, const char *value) {
	int idx = findKey (key);
	in_addr_t adr;

	if (idx < 0)
	{
		return (BAD_NUMBER);
	}

	if (curValues[idx].datatype != RMNVS_ADDR)
	{
		return (BAD_NUMBER);
	}
	int res=inet_aton(value, &adr);
	if (res==0) return(BAD_NUMBER);
	curValues[idx].changed=true;
	curValues[idx].curAddr = adr;
	return res;
}


/**
 * This can be used to get a look at the current state
 * of the in-core settings.
 * The user is responsible for decoding dta, according to
 * the data type returned.
 *
 * @param idx - the index of the item to be returned
 * @param changeFlag - pointer to location to store the 'changed' flag - 0 normally, 1 if this was changed but not saved.
 * @param keyName    - We will store a pointer to the name of this key here. DO NOT MODIFY the keyname!
 * @param dta        - pointer to the union where the data is stored.
 * @return           - we return the data type of this item. RMNVS_END if the index is out of range.
 *                     It is up to the caller to interpret the data in a reasonable fashion,
 */
RMNVS_DTA_t RmNvs::get_info (int idx, bool **changeFlag, const char **keyName,
		const void **dta)
{
	*keyName = curValues[idx].keyName;
	*dta = (void*) curValues[idx].curString;
	*changeFlag = &(curValues[idx].changed);
	return(curValues[idx].datatype);
}


/**
 * Get info about a specific entry as a string,
 * selected  by index. (used for 'show' command)
 */
const char* RmNvs::get_info (int idx)
{
	static char hdr[64];
	static char resp[128];
	char adrPtr[32];
	bzero(resp, sizeof(resp));
	sprintf(hdr, "%-15s Change Flag:%d ", curValues[idx].keyName, curValues[idx].changed);
	switch (curValues[idx].datatype)
	{
		case (RMNVS_STRING):
			sprintf (resp, "%s Type: String:   %s",hdr, curValues[idx].curString );
			break;

		case (RMNVS_INT):
			sprintf (resp, "%s Type: Integer:  %d",hdr, curValues[idx].curNumber );
			break;

		case (RMNVS_ADDR):
			RmNvs::get_addr_as_string (curValues[idx].keyName, adrPtr );
			sprintf (resp, "%s Type: Address:  %d (%s)", hdr, curValues[idx].curAddr,	adrPtr );
			break;

		case (RMNVS_BOOL):
			sprintf (resp, "%s Type: Bool:     %d", hdr, curValues[idx].curBool );
			break;

		case (RMNVS_END):
			bzero (resp, sizeof(resp) );
			break;
	}
	return (resp);
}

/**
 * For debugging - dump the current table.
 */
void RmNvs::dumpTable ()
{
	char buf[80];
	char hdr[80];
	for (int idx = 0; idx < NOOFCURVALUES; idx++ )
	{
		sprintf (hdr, "IDX: %3d Parameter: %s   changeFlag %d ", idx,
				curValues[idx].keyName, curValues[idx].changed );

		switch (curValues[idx].datatype)
		{
			case (RMNVS_STRING):
				ESP_LOGD(buf, "%s Type: String: %s", hdr, curValues[idx].curString );
				break;
			case (RMNVS_INT):
		ESP_LOGD(buf, "%s Type: Integer: %d",hdr, curValues[idx].curNumber );
				break;
			case (RMNVS_ADDR):
				RmNvs::get_addr_as_string (curValues[idx].keyName, buf );
			ESP_LOGD(buf, "%s Type: Address:%d (%s)",hdr, curValues[idx].curAddr, buf );
				break;

			case (RMNVS_BOOL):
		ESP_LOGD(buf, "%s Type: Bool:   %d",hdr, curValues[idx].curBool );
				break;

			case (RMNVS_END):
				break;
		}
	}
}
