/**
 * Arduino.cpp
 *
 *  Created on: Jan 11, 2022
 *      Author: doug
 *
 */

#include "Arduino.h"
#include "unistd.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/task.h"

static const char *TAG="Arduino.cpp-";
/**
 * Set the mode for the pin.
 * We assume no interrupt from the pin
 * Mode = INPUT, OUTPUT, or INPUT_PULLUP
 */
void pinMode(int pin,  enum IOMODE mode) {
	gpio_config_t cfg ={};

	if (pin==0) return;
	ESP_LOGI(TAG,"set pin mode for pin %d to %d",	pin, mode);

	cfg.pin_bit_mask= (1LL << pin);  // set the pin number
	cfg.pull_down_en=GPIO_PULLDOWN_DISABLE;

	if (mode == INPUT)
	{
		cfg.mode=GPIO_MODE_INPUT;
		cfg.pull_up_en=GPIO_PULLUP_DISABLE;
	}

	if (mode == INPUT_PULLUP)
	{
		cfg.mode=GPIO_MODE_INPUT;
		cfg.pull_up_en=GPIO_PULLUP_ENABLE;
	}

	if (mode == OUTPUT)
	{
		cfg.mode= GPIO_MODE_OUTPUT;
		cfg.pull_up_en=GPIO_PULLUP_DISABLE;
	}


	int ret=gpio_config(&cfg);
	if (ret!=ESP_OK) {
		ESP_LOGE(TAG, "pinmode for pin %d returned %d(%s)",pin, ret, esp_err_to_name(ret) );
		abort();
	}
}

/**
 * Set the level of the pin. dir is HIGH or LOW
 */
void digitalWrite(int pin, bool level) {
	if (pin==0) return; // Ignore pin 0
	gpio_set_level( (gpio_num_t)pin, level);
}

/**
 * Returns the time since startup in microseconds.
 * (Note: ESP 32 can keep 64 bit time - we truncate
 *    upper 16 bits to fit unsigned long)
 */
unsigned long micros() {
	int64_t now=esp_timer_get_time();
	unsigned long int res= now&0xFFFFFFFF; // Caution - loosing high bits
	return(res);
}

/**
 * Delay for a given number of microseconds.
 *
 * This MUST be kept short - we do not release
 * the CPU in this version (microseconds are too
 * short a time under RTOS).
 * The stepper uses this for less than 50 uSecs,
 * this should be okay.
 */
void delayMicroseconds(unsigned int us) {
	usleep(us);
}

/**
 * Delay for a given number of milliseconds.
 * NOTE: Unlike the Ardino version, this does NOT
 * stop other tasks or interrupts from happening.
 *
 * Note: NOT a precision measurement.
 *     This can result in another task getting control,
 *     resulting in us not returning for a longer-than-requested
 *     period.
 *
 */
void delay(unsigned long millsecs) {
	vTaskDelay( millsecs/portTICK_PERIOD_MS);
}


