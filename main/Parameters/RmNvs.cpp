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
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "nvs.h"
#include "RmNvs.h"


#define NVS_VERSION "1.0"



static bool have_init_ok = false;
static nvs_handle_t handle;
#define MAX_VALUES 15
static const char * NVS_PREFIX = "REMOTE_MOD";
static const char *TAG         = "+++NVS_ACCESS:";
static bool NvsIsSet=false;

#define EQUALSTR(_a_, _b_) (0==strcasecmp(_a_,_b_))

struct curValues_t {
	bool changed;           // If non-zero, this value was changes since last commmit.
	char keyName[15];       // Name of this key. No more than 15 chars
	RMNVS_DTA_t  datatype;  // How was this stored?
	union {
		char curString[32]; // Current value of this key (if string). Not longer than 32 chars
		int32_t curNumber;  // Current value of this key (if integer).
		// TODO: Add adresss type
	};
} ;

#define RMVS_END "END"

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
 * This initializes the values in the curValues structure
 * NOTE: The 'RMVNS_END' paramter MUST be the last in the sequence!
 */
void RmNvs::init_values() {

	int idx=0;
	initSingleString(idx++, RMNVS_KEY_WIFI_SSID,  "skulldougery");
	initSingleString(idx++, RMNVS_KEY_WIFI_PASS,  "password");
	initSingleString(idx++, RMNVS_FORCE_AP_MODE, "yes");
	initSingleInt   (idx++, RMNVS_USE_DHCP,       0);
	initSingleString(idx++, RMNVS_IP,             "0.0.0.0");
	initSingleString(idx++, RMNVS_NETMASK,        "255.255.255.0");
	initSingleString(idx++, RMNVS_ROUTER_ADDR,    " ");
	initSingleInt   (idx++, RMNVS_CMD_PORT,       3000);
	initSingleString(idx++, RMNVS_SRV_ADDR,       "192.168.1.2");
	initSingleInt   (idx++, RMNVS_SRV_PORT,       3001);
	initSingleString(idx++, RMNVS_DNS_ADDR,       "8.8.8.8");
	initSingleInt(idx++, RMNVS_WIFI_CHANNEL,      1);
	initSingleString(idx++, RMVS_END,             "END");
	curValues[idx].datatype=RMNVS_END;   // Force END flag.
	NOOFCURVALUES=idx;
};



/**
 * Turn a number-of-bits into a network mask.
 */
/* TODO:
esp_ip4_addr_t RMNVS_make_netmask(int  noOfBits) {
	esp_ip4_addr_t mask = {0};
	int bit;
	for (bit=0; bit<32; bit++) {
		if (bit<=noOfBits) {
			mask.addr |= 1;
		}
		mask.addr=mask.addr<<1;
	}
	return(mask);
}
*/



/**
 * Write any 'changed' values to NVS.
 */
void RmNvs::commit ()
{
	int idx;
	int err;
	ESP_LOGD(TAG, "IN RMNVS_commit" );
	for (idx = 0; idx < NOOFCURVALUES; idx++ )
	{
		err = ESP_OK;
		switch (curValues[idx].datatype)
		{
			case (RMNVS_STRING):

				ESP_LOGD(TAG,
						"RMNVS_commit: Processing STRING key %d name %s  value %s  changeFlag %d",
						idx, curValues[idx].keyName, curValues[idx].curString,
						curValues[idx].changed );

				if (curValues[idx].changed)
				{
					ESP_LOGD(TAG, "RMNVS_commit: saving key %s",
							curValues[idx].keyName );
					err = nvs_set_str (handle, curValues[idx].keyName,
							curValues[idx].curString );
				}
				break;

			case (RMNVS_INT):

				ESP_LOGD(TAG,
						"RMNVS_commit: Processing INTEGER key %d name %s  value %d  changeFlag %d",
						idx, curValues[idx].keyName, curValues[idx].curNumber,
						curValues[idx].changed );

				if (curValues[idx].changed)
				{
					ESP_LOGD(TAG, "RMNVS_commit: saving key %s",
							curValues[idx].keyName );
					err = nvs_set_i64 (handle, curValues[idx].keyName,
							curValues[idx].curNumber );
				}

				break;

			case(RMNVS_END):
				break;
		}

		if (err != ESP_OK)
		{
			// TODO: Something wicked this way comes!
			ESP_LOGE(TAG,
					"In RMNVS_commit - something wicked this way comes!  Key %d",
					idx );
			ESP_LOGE(TAG, "....KEY is %s", curValues[idx].keyName );
		}
		curValues[idx].changed = false;
	}
	return;
}

/**
 * This can be used to get a look at the current state
 * of the in-core settings.
 * The user is responsible for decoding dta, according to
 * the data type returned.
 * @param idx - the index of the item to be returned
 * @param changeFlag - pointer to location to store the 'changed' flag - 0 normally, 1 if this was changed but not saved.
 * @param keyName    - We will store a pointer to the name of this key here. DO NOT MODIFY the keyname!
 * @param dta        - pointer to the union where the data is stored.
 * @return           - we return the data type of this item. RMNVS_END if the index is out of range.
 *                     It is up to the caller to interpret the data in a reasonable fashion,
 */
RMNVS_DTA_t RmNvs::get_info (int idx, bool *changeFlag, const char *keyName,
		const void *dta)
{
	keyName = curValues[idx].keyName;
	dta = (void*) curValues[idx].curString;
	*changeFlag = curValues[idx].changed;
	return(curValues[idx].datatype);
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
void RmNvs::load_from_nvs() {
	int idx;
	int err=ESP_OK;

	for (idx=0; idx<NOOFCURVALUES; idx++) {
		switch(curValues[idx].datatype) {
			case(RMNVS_STRING):
					size_t len;
					err=nvs_get_str(handle, curValues[idx].keyName,
					   curValues[idx].curString, &len);
			break;

			case(RMNVS_INT):
				err=nvs_get_i32(handle, curValues[idx].curString, &(curValues[idx].curNumber) );
					break;

			case (RMNVS_END ):
					break;
		}
		if (err==ESP_OK) curValues[idx].changed=false;
	}
	return;
}

/**
 * Indicate if NVS was set.  If set, then
 * we will jump into configuration mode upon startup.
 */
bool RmNvs::nvs_isSet(void) {
	return(NvsIsSet);
}


/**
 * Initialize the NVS memory system.
 * Only called once on startup.
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
	init_values(); // give our in-core array its default values.

	have_init_ok = true;
	ESP_LOGD(TAG, "In RmNvs init");
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// NVS partition was truncated and/or needs to be erased
		// Retry nvs_flash_init
		ESP_LOGD(TAG, "Erase Flash and re-init");
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();

	}
	ESP_ERROR_CHECK(err);  // Report any remaining error (should be none!)
	ESP_ERROR_CHECK(nvs_open(NVS_PREFIX, NVS_READWRITE, &handle));

	/*
	 *  try to acces RMNVS_KEY_WIFI_SSID.
	 * If not present, set NO_CONFIG flag.
	 */
	if (forceReset  ||  ESP_OK != nvs_get_str(handle, RMVS_END, flag, &flen) ) {
		// Initialize using defaults if we dont have the RMNVS_INITIALIZED flag.
		ESP_LOGD(TAG, " Reset NVS ");
		NvsIsSet=false;
	}  else {
		// Get any existing settings, then commit defaults for any that aren't in NVS.
		ESP_LOGD(TAG, " Use existing NVS content");
		load_from_nvs();
		NvsIsSet=true;
	}

// For debugging, override the NvsIsSet flag
#ifdef DEBUG_STARTUP_STA
	NvsIsSet=true;
#endif
#ifdef DEBUG_STARTUP_AP
	NvsIsSet=false;
#endif

	ESP_LOGD(TAG, "Leave RmNvs init. NvsIsSet=%d",NvsIsSet );

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
	ESP_LOGD(TAG, "findKey failed to find key %s", keyname);
	return (-1);
}


/**
 * Get the config value from NVS  as a string, based on its key
 * We return a pointert to a CONSTANT - dont modify it, or try
 * to 'free' it.
 */
const char *RmNvs::get_str(const char *key) {
	int idx=findKey(key);

	if (idx<0) {
		ESP_LOGD(TAG, "In RMNVS_get: findKey returned %d for key %s", idx, key);
		return(NULL);
	}
	return(curValues[idx].curString);
}

/**
 * Set the config value.
 * This silently truncates if the value is too long!
 */
void RmNvs::set_str(const char *key, const char *value) {
	int idx=findKey(key);
	// dont free any longer!    free(curValues[idx].curValue);
	strncpy(curValues[idx].curString, value, sizeof(curValues[idx].curString));
	curValues[idx].changed=true;
	return;
}

/**
 * Get the requested value as an integer.
 *
 * Returns BAD_NUMBER (from messages.h) if any error
 */
int  RMNVS_get_number(const char *key) {
	int res=validInt( RMNVS_get(key), true);
	if (res==BAD_NUMBER) {
		ESP_LOGE(TAG, "ERROR: RMNVS returned bad number for key %s", key);
		abort();
	}
	return(res);
}

/**
 * Set a numeric value.
 *   (the value is assumed to be an int )
 */
void RMNVS_set_number(const char *key, int value) {
	char buf[64];
	sprintf(buf, "%d", value);
	RMNVS_set(key, buf);
}
